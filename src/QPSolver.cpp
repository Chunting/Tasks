// This file is part of Tasks.
//
// Tasks is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Tasks is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Tasks.  If not, see <http://www.gnu.org/licenses/>.

// associated header
#include "QPSolver.h"

// includes
// std
#include <limits>
#include <cmath>
// RBDyn
#include <RBDyn/MultiBody.h>
#include <RBDyn/MultiBodyConfig.h>
#include <RBDyn/EulerIntegration.h>
#include <RBDyn/FK.h>
#include <RBDyn/FV.h>

// Tasks
#include "QL.h"



namespace tasks
{

namespace qp
{


// Value add to the diagonal to ensure positive matrix
static const double DIAG_CONSTANT = 1e-5;


/**
	*													QPSolver
	*/



QPSolver::QPSolver(bool silent):
  constr_(),
  eqConstr_(),
  inEqConstr_(),
  boundConstr_(),
  tasks_(),
  nrEq_(0),
  A1_(),B1_(),
  nrInEq_(0),
  A2_(),B2_(),
  XL_(),XU_(),
  Q_(),C_(),
  res_(),
  silent_(silent)
{
  lssol_.warm(false);
}


bool QPSolver::update(const rbd::MultiBody& mb, rbd::MultiBodyConfig& mbc, double step)
{
  return updateLSSOL(mb, mbc, step);
}


bool QPSolver::updateQLD(const rbd::MultiBody& mb, rbd::MultiBodyConfig& mbc, double step)
{
	preUpdate(mb, mbc);

	bool success = false;
	double iter = 1e-8;
	while(!success && iter < 1e-3)
	{
		success = qld_.solve(Q_, C_,
			A1_.block(0, 0, nrEq_, data_.nrVars_), B1_.head(nrEq_),
			A2_.block(0, 0, nrInEq_, data_.nrVars_), B2_.head(nrInEq_),
			XL_, XU_, iter);
		iter *= 10.;
	}
	postUpdate(mb, mbc, success, qld_.result(), step);

	/*
	while(!success && iter < 1e-3)
	{
		success = solveQP(A1_.cols(), A1_.rows(), A2_.rows(),
			Q_, C_, A1_, B1_, A2_, B2_, XL_, XU_, res_, iter, silent_);
		iter *= 10.;
	}
	postUpdate(mb, mbc, success, res_);
	*/

	return success;
}


bool QPSolver::updateLSSOL(const rbd::MultiBody& mb, rbd::MultiBodyConfig& mbc, double step)
{
	preUpdate(mb, mbc);

	bool success = lssol_.solve(Q_, C_,
		A1_.block(0, 0, nrEq_, data_.nrVars_), B1_.head(nrEq_),
		A2_.block(0, 0, nrInEq_, data_.nrVars_), B2_.head(nrInEq_),
		XL_, XU_);

	postUpdate(mb, mbc, success, lssol_.result(), step);

	return success;
}


void QPSolver::updateEqConstrSize()
{
	int nbEq = 0;
	for(std::size_t i = 0; i < eqConstr_.size(); ++i)
	{
		nbEq += eqConstr_[i]->maxEq();
	}

	nrEq_ = 0;
	A1_.resize(nbEq, data_.nrVars_);
	B1_.resize(nbEq);

	updateSolverSize(data_.nrVars_, nbEq, int(B2_.rows()));
}


void QPSolver::updateInEqConstrSize()
{
	int nbInEq = 0;
	for(std::size_t i = 0; i < inEqConstr_.size(); ++i)
	{
		nbInEq += inEqConstr_[i]->maxInEq();
	}

	nrInEq_ = 0;
	A2_.resize(nbInEq, data_.nrVars_);
	B2_.resize(nbInEq);

	updateSolverSize(data_.nrVars_, int(B1_.rows()), nbInEq);
}


void QPSolver::nrVars(const rbd::MultiBody& mb,
	std::vector<UnilateralContact> uni,
	std::vector<BilateralContact> bi,
	std::vector<UnilateralContact> robToManip,
	std::vector<UnilateralContact> manipToRob)
{
	data_.alphaD_ = mb.nrDof();
	data_.lambda_ = 0;
	data_.lambdaUni_ = 0;
	data_.lambdaBi_ = 0;
	data_.lambdaManip_ = 0;
	data_.torque_ = (mb.nrDof() - mb.joint(0).dof());
	data_.uniCont_ = uni;
	data_.biCont_ = bi;

	data_.robotToManipBodyContacts(robToManip);
	data_.manipBodyToRobotContacts(manipToRob);

	// counting unilateral contact
	for(const UnilateralContact& c: data_.uniCont_)
	{
		for(std::size_t i = 0; i < c.points.size(); ++i)
		{
			data_.lambdaUni_ += c.nrLambda(int(i));
		}
	}

	// counting bilateral contact
	for(const BilateralContact& c: data_.biCont_)
	{
		for(std::size_t i = 0; i < c.points.size(); ++i)
		{
			data_.lambdaBi_ += c.nrLambda(int(i));
		}
	}

	// counting robot to manipulated body contacts
	for(const UnilateralContact& c: data_.robotToManipBodyContacts())
	{
		for(std::size_t i = 0; i < c.points.size(); ++i)
		{
			data_.lambdaManip_ += c.nrLambda(int(i));
		}
	}
	
	if(data_.lambdaManip_!=0)
	{
		data_.alphaD_+=6; //Add the free flyer of the manipulated body
	}

	//Lambda = uni + bi + manip
	data_.lambda_ = data_.lambdaUni_ + data_.lambdaBi_ + data_.lambdaManip_; 
	data_.nrVars_ = data_.alphaD_ + data_.lambda_ + data_.torque_;

	if(XL_.rows() != data_.nrVars_)
	{
		XL_.resize(data_.nrVars_);
		XU_.resize(data_.nrVars_);

		Q_.resize(data_.nrVars_, data_.nrVars_);
		C_.resize(data_.nrVars_);

		res_.resize(data_.nrVars_);
		torqueRes_.resize(mb.nrDof());
		if(mb.joint(0).type() == rbd::Joint::Free)
		{
			torqueRes_.segment(0, mb.joint(0).dof()).setZero();
		}
	}

	for(Task* t: tasks_)
	{
		t->updateNrVars(mb, data_);
	}

	for(Constraint* c: constr_)
	{
		c->updateNrVars(mb, data_);
	}

	updateSolverSize(data_.nrVars_, static_cast<int>(B1_.rows()),
		static_cast<int>(B2_.rows()));
}


int QPSolver::nrVars() const
{
	return data_.nrVars_;
}


void QPSolver::addEqualityConstraint(Equality* co)
{
	eqConstr_.push_back(co);
}


void QPSolver::removeEqualityConstraint(Equality* co)
{
	eqConstr_.erase(std::find(eqConstr_.begin(), eqConstr_.end(), co));
}


int QPSolver::nrEqualityConstraints() const
{
	return static_cast<int>(eqConstr_.size());
}


void QPSolver::addInequalityConstraint(Inequality* co)
{
	inEqConstr_.push_back(co);
}


void QPSolver::removeInequalityConstraint(Inequality* co)
{
	inEqConstr_.erase(std::find(inEqConstr_.begin(), inEqConstr_.end(), co));
}


int QPSolver::nrInequalityConstraints() const
{
	return static_cast<int>(inEqConstr_.size());
}


void QPSolver::addBoundConstraint(Bound* co)
{
	boundConstr_.push_back(co);
}


void QPSolver::removeBoundConstraint(Bound* co)
{
	boundConstr_.erase(std::find(boundConstr_.begin(), boundConstr_.end(), co));
}


int QPSolver::nrBoundConstraints() const
{
	return static_cast<int>(boundConstr_.size());
}


void QPSolver::addConstraint(Constraint* co)
{
	if(std::find(constr_.begin(), constr_.end(), co) == constr_.end())
	{
		constr_.push_back(co);
	}
}


void QPSolver::removeConstraint(Constraint* co)
{
	auto it = std::find(constr_.begin(), constr_.end(), co);
	if(it != constr_.end())
	{
		constr_.erase(it);
	}
}

int QPSolver::nrConstraints() const
{
	return static_cast<int>(constr_.size());
}


void QPSolver::addTask(Task* task)
{
	if(std::find(tasks_.begin(), tasks_.end(), task) == tasks_.end())
	{
		tasks_.push_back(task);
	}
}


void QPSolver::removeTask(Task* task)
{
	auto it = std::find(tasks_.begin(), tasks_.end(), task);
	if(it != tasks_.end())
	{
		tasks_.erase(it);
	}
}


int QPSolver::nrTasks() const
{
	return static_cast<int>(tasks_.size());
}


void QPSolver::resetTasks()
{
	tasks_.clear();
}


const Eigen::VectorXd& QPSolver::result() const
{
	return res_;
}


Eigen::VectorXd QPSolver::alphaDVec() const
{
	return res_.head(data_.alphaD_);
}


Eigen::VectorXd QPSolver::lambdaVec() const
{
	return res_.segment(data_.alphaD_, data_.lambda_);
}


Eigen::VectorXd QPSolver::torqueVec() const
{
	return res_.tail(data_.torque_);
}


int QPSolver::contactLambdaPosition(int bodyId) const
{
	int pos = 0;
	for(const UnilateralContact& uc: data_.unilateralContacts())
	{
		if(uc.bodyId == bodyId)
		{
			return pos;
		}

		for(std::size_t i = 0; i < uc.points.size(); ++i)
		{
			pos += uc.nrLambda(int(i));
		}
	}

	for(const BilateralContact& bc: data_.bilateralContacts())
	{
		if(bc.bodyId == bodyId)
		{
			return pos;
		}

		for(std::size_t i = 0; i < bc.points.size(); ++i)
		{
			pos += bc.nrLambda(int(i));
		}
	}

	return -1;
}


void QPSolver::updateSolverSize(int nrVar, int nrEq, int nrIneq)
{
	updateLSSOLSize(nrVar, nrEq, nrIneq);
}


void QPSolver::updateQLDSize(int nrVar, int nrEq, int nrIneq)
{
	qld_.problem(nrVar, nrEq, nrIneq);
}


void QPSolver::updateLSSOLSize(int nrVar, int nrEq, int nrIneq)
{
	lssol_.problem(nrVar, nrEq, nrIneq);
	// warning, if the user change the number of dof of the robot those lines
	// don't have any sense
	/*
	lssol_.result().head(data_.alphaD_) = res_.head(data_.alphaD_);
	lssol_.result().tail(data_.torque_) = res_.tail(data_.torque_);
	*/
}


void QPSolver::preUpdate(const rbd::MultiBody& mb, rbd::MultiBodyConfig& mbc)
{
	for(std::size_t i = 0; i < constr_.size(); ++i)
	{
		constr_[i]->update(mb, mbc);
	}

	for(std::size_t i = 0; i < tasks_.size(); ++i)
	{
		tasks_[i]->update(mb, mbc);
	}

	A1_.setZero();
	B1_.setZero();
	A2_.setZero();
	B2_.setZero();
	XL_.fill(-std::numeric_limits<double>::infinity());
	XU_.fill(std::numeric_limits<double>::infinity());
	Q_.setZero();
	C_.setZero();

	nrEq_ = 0;
	for(std::size_t i = 0; i < eqConstr_.size(); ++i)
	{
		// eq constraint can return a matrix with more line
		// than the number of constraint
		int nrConstr = eqConstr_[i]->nrEq();
		const Eigen::MatrixXd& A1 = eqConstr_[i]->AEq();
		const Eigen::VectorXd& B1 = eqConstr_[i]->BEq();

		A1_.block(nrEq_, 0, nrConstr, data_.nrVars_) =
			A1.block(0, 0, nrConstr, data_.nrVars_);
		B1_.segment(nrEq_, nrConstr) = B1.head(nrConstr);

		nrEq_ += nrConstr;
	}

	nrInEq_ = 0;
	for(std::size_t i = 0; i < inEqConstr_.size(); ++i)
	{
		// ineq constraint can return a matrix with more line
		// than the number of constraint
		int nrConstr = inEqConstr_[i]->nrInEq();
		const Eigen::MatrixXd& A2 = inEqConstr_[i]->AInEq();
		const Eigen::VectorXd& B2 = inEqConstr_[i]->BInEq();

		A2_.block(nrInEq_, 0, nrConstr, data_.nrVars_) =
			A2.block(0, 0, nrConstr, data_.nrVars_);
		B2_.segment(nrInEq_, nrConstr) = B2.head(nrConstr);

		nrInEq_ += nrConstr;
	}

	for(std::size_t i = 0; i < boundConstr_.size(); ++i)
	{
		const Eigen::VectorXd& XL = boundConstr_[i]->Lower();
		const Eigen::VectorXd& XU = boundConstr_[i]->Upper();
		int bv = boundConstr_[i]->beginVar();

		XL_.segment(bv, XL.size()) = XL;
		XU_.segment(bv, XU.size()) = XU;
	}

	for(std::size_t i = 0; i < tasks_.size(); ++i)
	{
		const Eigen::MatrixXd& Q = tasks_[i]->Q();
		const Eigen::VectorXd& C = tasks_[i]->C();
		std::pair<int, int> b = tasks_[i]->begin();

		int r = static_cast<int>(Q.rows());
		int c = static_cast<int>(Q.cols());

		Q_.block(b.first, b.second, r, c) += tasks_[i]->weight()*Q;
		C_.segment(b.first, r) += tasks_[i]->weight()*C;
	}

	// try to transform Q_ to a positive matrix
	// we just add a small value to the diagonal since
	// the first necessary condition is to have
	// Q_(i,i) > 0
	// may be we can try to check the second
	// condition in a near futur
	// Q_(i,i) + Q_(j,j) > 2·Q_(i,j) for i≠j
	for(int i = 0; i < data_.nrVars_; ++i)
	{
		if(std::abs(Q_(i, i)) < DIAG_CONSTANT)
		{
			Q_(i, i) += DIAG_CONSTANT;
		}
	}
}


void QPSolver::postUpdate(const rbd::MultiBody& mb,
	rbd::MultiBodyConfig& mbc, bool success, const Eigen::VectorXd& result, double step)
{
	if(success)
	{
		res_ = result;
		int dof0 = mb.joint(0).dof();
		torqueRes_.segment(dof0, mb.nrDof() - dof0) = result.tail(data_.torque_);

		rbd::vectorToParam(torqueRes_, mbc.jointTorque);
		// don't write contact force to the structure since contact force are used
		// to compute C vector.
		//Write manipulated object if it exists
		if(data_.robotToManipBodyContacts().size()!=0)
		{
			rbd::vectorToParam(res_.head(data_.alphaD_-6), mbc.alphaD);
			rbd::vectorToParam(res_.segment(data_.alphaD_-6,6), data_.manipBodyConfig_.alphaD);
			rbd::eulerIntegration(data_.manipBody_, data_.manipBodyConfig_, step);
			rbd::forwardKinematics(data_.manipBody_, data_.manipBodyConfig_);
			rbd::forwardVelocity(data_.manipBody_, data_.manipBodyConfig_);
		}
		else
		{
			rbd::vectorToParam(res_.head(data_.alphaD_), mbc.alphaD);
		}
	}
}

void QPSolver::manipBody(const rbd::MultiBody& body)
{
	data_.manipBody(body);
}

void QPSolver::manipBody(const rbd::MultiBody& body, const rbd::MultiBodyConfig& bodyConfig)
{
	data_.manipBody(body);
	data_.manipBodyConfig(bodyConfig);
}

void QPSolver::manipBodyConfig(const rbd::MultiBodyConfig& body)
{
	data_.manipBodyConfig(body);
}

rbd::MultiBodyConfig QPSolver::manipBodyConfig()
{
	return data_.manipBodyConfig();
}

} // namespace qp

} // namespace tasks
