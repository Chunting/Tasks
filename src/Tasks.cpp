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
#include "Tasks.h"

// includes
// std
#include <set>
#include <iostream>
// rbd
#include <RBDyn/MultiBody.h>
#include <RBDyn/MultiBodyConfig.h>
#include <RBDyn/FK.h>
#include <RBDyn/FV.h>
namespace tasks
{


/**
	*													PositionTask
	*/


PositionTask::PositionTask(const rbd::MultiBody& mb, int bodyId,
  const Eigen::Vector3d& pos, const Eigen::Vector3d& bodyPoint):
  pos_(pos),
  point_(bodyPoint),
  bodyIndex_(mb.bodyIndexById(bodyId)),
  jac_(mb, bodyId, bodyPoint),
  eval_(3),
  shortJacMat_(3, jac_.dof()),
  jacMat_(3, mb.nrDof()),
  jacDotMat_(3, mb.nrDof())
{
}


void PositionTask::position(const Eigen::Vector3d& pos)
{
	pos_ = pos;
}


const Eigen::Vector3d& PositionTask::position() const
{
	return pos_;
}


void PositionTask::bodyPoint(const Eigen::Vector3d& point)
{
	jac_.point(point);
}


const Eigen::Vector3d& PositionTask::bodyPoint() const
{
	return jac_.point();
}


void PositionTask::update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	eval_ = pos_ - (point_*mbc.bodyPosW[bodyIndex_]).translation();

	shortJacMat_ = jac_.jacobian(mb, mbc).block(3, 0, 3, shortJacMat_.cols());
	jac_.fullJacobian(mb, shortJacMat_, jacMat_);
}


void PositionTask::updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	shortJacMat_ = jac_.jacobianDot(mb, mbc).block(3, 0, 3, shortJacMat_.cols());
	jac_.fullJacobian(mb, shortJacMat_, jacDotMat_);
}


const Eigen::VectorXd& PositionTask::eval() const
{
	return eval_;
}


const Eigen::MatrixXd& PositionTask::jac() const
{
	return jacMat_;
}


const Eigen::MatrixXd& PositionTask::jacDot() const
{
	return jacDotMat_;
}


/**
	*													OrientationTask
	*/


OrientationTask::OrientationTask(const rbd::MultiBody& mb, int bodyId, const Eigen::Quaterniond& ori):
  ori_(ori.matrix()),
  bodyIndex_(mb.bodyIndexById(bodyId)),
  jac_(mb, bodyId),
  eval_(3),
  shortJacMat_(3, jac_.dof()),
  jacMat_(3, mb.nrDof()),
  jacDotMat_(3, mb.nrDof())
{
}


OrientationTask::OrientationTask(const rbd::MultiBody& mb, int bodyId, const Eigen::Matrix3d& ori):
  ori_(ori),
  bodyIndex_(mb.bodyIndexById(bodyId)),
  jac_(mb, bodyId),
  eval_(3),
  shortJacMat_(3, jac_.dof()),
  jacMat_(3, mb.nrDof()),
  jacDotMat_(3, mb.nrDof())
{
}


void OrientationTask::orientation(const Eigen::Quaterniond& ori)
{
	ori_ = ori.matrix();
}


void OrientationTask::orientation(const Eigen::Matrix3d& ori)
{
	ori_ = ori;
}


const Eigen::Matrix3d& OrientationTask::orientation() const
{
	return ori_;
}


void OrientationTask::update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	eval_ = sva::rotationError(mbc.bodyPosW[bodyIndex_].rotation(), ori_, 1e-7);

	shortJacMat_ = jac_.jacobian(mb, mbc).block(0, 0, 3, shortJacMat_.cols());
	jac_.fullJacobian(mb, shortJacMat_, jacMat_);
}


void OrientationTask::updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	shortJacMat_ = jac_.jacobianDot(mb, mbc).block(0, 0, 3, shortJacMat_.cols());
	jac_.fullJacobian(mb, shortJacMat_, jacDotMat_);
}


const Eigen::VectorXd& OrientationTask::eval() const
{
	return eval_;
}


const Eigen::MatrixXd& OrientationTask::jac() const
{
	return jacMat_;
}


const Eigen::MatrixXd& OrientationTask::jacDot() const
{
	return jacDotMat_;
}


/**
	*													PostureTask
	*/


PostureTask::PostureTask(const rbd::MultiBody& mb, std::vector<std::vector<double> > q):
	q_(q),
	eval_(mb.nrDof()),
	jacMat_(mb.nrDof(), mb.nrDof()),
	jacDotMat_(mb.nrDof(), mb.nrDof())
{
	eval_.setZero();
	jacMat_.setIdentity();
	jacDotMat_.setZero();

	if(mb.nrDof() > 0 && mb.joint(0).type() == rbd::Joint::Free)
	{
		for(int i = 0; i < 6; ++i)
		{
			jacMat_(i, i) = 0;
		}
	}
}


void PostureTask::posture(std::vector<std::vector<double> > q)
{
	q_ = q;
}


const std::vector<std::vector<double> > PostureTask::posture() const
{
	return q_;
}


void PostureTask::update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	using namespace Eigen;

	int pos = mb.jointPosInDof(1);

	// we drop the first joint (fixed or free flyier).
	for(int i = 1; i < mb.nrJoints(); ++i)
	{
		// if dof == 1 is a prismatic/revolute joint
		// else if dof == 4 is a spherical one
		// else is a fixed one
		if(mb.joint(i).dof() == 1)
		{
			eval_(pos) = q_[i][0] - mbc.q[i][0];
			++pos;
		}
		else if(mb.joint(i).dof() == 4)
		{
			Matrix3d orid(
				Quaterniond(q_[i][0], q_[i][1], q_[i][2], q_[i][3]).matrix());

			Vector3d err = sva::rotationError(mbc.jointConfig[i].rotation(), orid);

			eval_.segment(pos, 3) = err;
			pos += 3;
		}
	}
}


void PostureTask::updateDot(const rbd::MultiBody& /* mb */, const rbd::MultiBodyConfig& /* mbc */)
{}


const Eigen::VectorXd& PostureTask::eval() const
{
	return eval_;
}


const Eigen::MatrixXd& PostureTask::jac() const
{
	return jacMat_;
}


const Eigen::MatrixXd& PostureTask::jacDot() const
{
	return jacDotMat_;
}


/**
	*													CoMTask
	*/


CoMTask::CoMTask(const rbd::MultiBody& mb, const Eigen::Vector3d& com):
  com_(com),
  jac_(mb),
  eval_(3),
  jacMat_(3, mb.nrDof()),
  jacDotMat_(3, mb.nrDof())
{}


void CoMTask::com(const Eigen::Vector3d& com)
{
  com_ = com;
}


const Eigen::Vector3d CoMTask::com() const
{
  return com_;
}


void CoMTask::update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	eval_ = com_ - rbd::computeCoM(mb, mbc);

	jacMat_ = jac_.jacobian(mb, mbc).block(3, 0, 3, mb.nrDof());
}


void CoMTask::updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	jacDotMat_ = jac_.jacobianDot(mb, mbc).block(3, 0, 3, mb.nrDof());
}


const Eigen::VectorXd& CoMTask::eval() const
{
	return eval_;
}


const Eigen::MatrixXd& CoMTask::jac() const
{
	return jacMat_;
}


const Eigen::MatrixXd& CoMTask::jacDot() const
{
	return jacDotMat_;
}

ManipCoMTask::ManipCoMTask(const rbd::MultiBody& mb, const Eigen::Vector3d& com,
		const rbd::MultiBody& mbManip, int bodyIdContact,
		const sva::PTransformd& toSurface):
  com_(com),
  eval_(3),
  jacMat_(3, mb.nrDof()),
  jacDotMat_(3, mb.nrDof()),
  mbManip_(),
  mbcManip_()
{
	std::vector<rbd::Body> bodies(mb.bodies());
	std::vector<rbd::Joint> joints(mb.joints());
	std::vector<int> pred(mb.predecessors());
	std::vector<int> succ(mb.successors());
	std::vector<int> parent(mb.parents());
	std::vector<sva::PTransformd> Xt(mb.transforms());
	rbd::Body newBody; //Create copy of mbManip with new index
	rbd::Joint newJoint; //Create new fixed joint

	int bodyIndex = bodies.size();
	int jointIndex = joints.size();

	newBody = rbd::Body(mbManip.body(0).inertia(), 15000, "ManipBody");
	newJoint = rbd::Joint(rbd::Joint::Fixed, true, 42000, "ManipJoint");

	bodies.push_back(newBody);
	joints.push_back(newJoint);

	pred.push_back(mb.bodyIndexById(bodyIdContact));
	succ.push_back(bodyIndex);
	parent.push_back(mb.bodyIndexById(bodyIdContact));

	Xt.push_back(toSurface);

	mbManip_ = rbd::MultiBody(bodies, joints, pred, succ, parent, Xt);
	mbcManip_ = rbd::MultiBodyConfig(mbManip_);
	mbcManip_.zero(mbManip_);

	std::vector<double> weights(mbManip_.nrBodies(), 1.);
	weights[bodyIndex] = 0.001;

	jac_ = rbd::CoMJacobianDummy(mbManip_, weights);
}


void ManipCoMTask::com(const Eigen::Vector3d& com)
{
  com_ = com;
}


const Eigen::Vector3d ManipCoMTask::com() const
{
  return com_;
}


void ManipCoMTask::update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	fillMbcManip(mbc);
	eval_ = com_ - rbd::computeCoM(mbManip_, mbcManip_);

	jacMat_ = jac_.jacobian(mbManip_, mbcManip_).block(3, 0, 3, mb.nrDof());
}


void ManipCoMTask::updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	fillMbcManip(mbc);
	jacDotMat_ = jac_.jacobianDot(mbManip_, mbcManip_).block(3, 0, 3, mb.nrDof());
}

void ManipCoMTask::fillMbcManip(const rbd::MultiBodyConfig& mbc)
{
	int i = 0;
	for(auto vec: mbc.q)
	{
		mbcManip_.q[i] = vec;
		++i;
	}
	rbd::forwardKinematics(mbManip_, mbcManip_);
	rbd::forwardVelocity(mbManip_, mbcManip_);
}

const Eigen::VectorXd& ManipCoMTask::eval() const
{
	return eval_;
}


const Eigen::MatrixXd& ManipCoMTask::jac() const
{
	return jacMat_;
}


const Eigen::MatrixXd& ManipCoMTask::jacDot() const
{
	return jacDotMat_;
}

const rbd::MultiBody& ManipCoMTask::mbTask() const
{
	return mbManip_;
}

const rbd::MultiBodyConfig& ManipCoMTask::mbcTask() const
{
	return mbcManip_;
}

MomentumTask::MomentumTask(const rbd::MultiBody& mb, const sva::ForceVecd mom):
  momentum_(mom),
  momentumMatrix_(mb),
  eval_(6),
  jacMat_(6,mb.nrDof()),
  jacDotMat_(6,mb.nrDof())
{
}

void MomentumTask::momentum(const sva::ForceVecd& mom)
{
	momentum_ = mom;
}

const sva::ForceVec<double> MomentumTask::momentum() const
{
	return momentum_;
}

void MomentumTask::update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	Eigen::Vector3d com = rbd::computeCoM(mb, mbc);

	eval_ = momentum_.vector() - rbd::computeCentroidalMomentum(mb, mbc, com).vector();

	momentumMatrix_.computeMatrix(mb, mbc, com);

	jacMat_ = momentumMatrix_.matrix();
}

void MomentumTask::updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	momentumMatrix_.computeMatrixDot(mb, mbc, rbd::computeCoM(mb, mbc),
				rbd::computeCoMVelocity(mb, mbc));
	jacDotMat_ = momentumMatrix_.matrixDot();
}

void MomentumTask::updateAll(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	Eigen::Vector3d com = rbd::computeCoM(mb, mbc);

	eval_ = momentum_.vector() - rbd::computeCentroidalMomentum(mb, mbc, com).vector();

	momentumMatrix_.computeMatrixAndMatrixDot(mb, mbc, com,	rbd::computeCoMVelocity(mb, mbc));
	jacMat_ = momentumMatrix_.matrix();
	jacDotMat_ = momentumMatrix_.matrixDot();
}

const Eigen::VectorXd& MomentumTask::eval() const
{
	return eval_;
}

const Eigen::MatrixXd& MomentumTask::jac() const
{
	return jacMat_;
}

const Eigen::MatrixXd& MomentumTask::jacDot() const
{
	return jacDotMat_;
}

/**
	*													LinVelocityTask
	*/


LinVelocityTask::LinVelocityTask(const rbd::MultiBody& mb, int bodyId,
  const Eigen::Vector3d& v, const Eigen::Vector3d& bodyPoint):
  vel_(v),
  point_(bodyPoint),
  bodyIndex_(mb.bodyIndexById(bodyId)),
  jac_(mb, bodyId, bodyPoint),
  eval_(3),
  shortJacMat_(3, jac_.dof()),
  jacMat_(3, mb.nrDof()),
  jacDotMat_(3, mb.nrDof())
{
}


void LinVelocityTask::velocity(const Eigen::Vector3d& v)
{
	vel_ = v;
}


const Eigen::Vector3d& LinVelocityTask::velocity() const
{
	return vel_;
}


void LinVelocityTask::bodyPoint(const Eigen::Vector3d& point)
{
	jac_.point(point);
}


const Eigen::Vector3d& LinVelocityTask::bodyPoint() const
{
	return jac_.point();
}


void LinVelocityTask::update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	sva::PTransformd E_0_b(mbc.bodyPosW[bodyIndex_].rotation());
	eval_ = vel_ - (E_0_b.invMul(point_*mbc.bodyVelB[bodyIndex_])).linear();

	shortJacMat_ = jac_.jacobian(mb, mbc).block(3, 0, 3, shortJacMat_.cols());
	jac_.fullJacobian(mb, shortJacMat_, jacMat_);
}


void LinVelocityTask::updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc)
{
	shortJacMat_ = jac_.jacobianDot(mb, mbc).block(3, 0, 3, shortJacMat_.cols());
	jac_.fullJacobian(mb, shortJacMat_, jacDotMat_);
}


const Eigen::VectorXd& LinVelocityTask::eval() const
{
	return eval_;
}


const Eigen::MatrixXd& LinVelocityTask::jac() const
{
	return jacMat_;
}


const Eigen::MatrixXd& LinVelocityTask::jacDot() const
{
	return jacDotMat_;
}


/**
	*													OrientationTrackingTask
	*/


OrientationTrackingTask::OrientationTrackingTask(const rbd::MultiBody& mb,
	int bodyId, const Eigen::Vector3d& bodyPoint, const Eigen::Vector3d& bodyAxis,
	const std::vector<int>& trackingJointsId,
	const Eigen::Vector3d& trackedPoint):
	bodyIndex_(mb.bodyIndexById(bodyId)),
	bodyPoint_(bodyPoint),
	bodyAxis_(bodyAxis),
	zeroJacIndex_(),
	trackedPoint_(trackedPoint),
	jac_(mb, bodyId),
	eval_(3),
	shortJacMat_(3, jac_.dof()),
	jacMat_(3, mb.nrDof()),
	jacDotMat_(3, mb.nrDof())
{
	std::set<int> trackingJointsIndex;
	for(int id: trackingJointsId)
	{
		trackingJointsIndex.insert(mb.jointIndexById(id));
	}

	int jacPos = 0;
	for(int i: jac_.jointsPath())
	{
		const rbd::Joint& curJoint = mb.joint(i);
		if(trackingJointsIndex.find(i) == std::end(trackingJointsIndex))
		{
			for(int j = 0; j < curJoint.dof(); ++j)
			{
				zeroJacIndex_.push_back(jacPos + j);
			}
		}

		jacPos += curJoint.dof();
	}
}


void OrientationTrackingTask::trackedPoint(const Eigen::Vector3d& tp)
{
	trackedPoint_ = tp;
}


const Eigen::Vector3d& OrientationTrackingTask::trackedPoint() const
{
	return trackedPoint_;
}


void OrientationTrackingTask::bodyPoint(const Eigen::Vector3d& bp)
{
	bodyPoint_ = sva::PTransformd(bp);
}


const Eigen::Vector3d& OrientationTrackingTask::bodyPoint() const
{
	return bodyPoint_.translation();
}


void OrientationTrackingTask::bodyAxis(const Eigen::Vector3d& ba)
{
	bodyAxis_ = ba;
}


const Eigen::Vector3d& OrientationTrackingTask::bodyAxis() const
{
	return bodyAxis_;
}


void OrientationTrackingTask::update(const rbd::MultiBody& mb,
	const rbd::MultiBodyConfig& mbc)
{
	using namespace Eigen;
	const sva::PTransformd& bodyTf = mbc.bodyPosW[bodyIndex_];
	Vector3d desDir(trackedPoint_ - (bodyPoint_*bodyTf).translation());
	Vector3d curDir(bodyTf.rotation().transpose()*bodyAxis_);
	desDir.normalize();
	curDir.normalize();

	Matrix3d targetOri(
		Quaterniond::FromTwoVectors(curDir, desDir).inverse().matrix());

	eval_ = sva::rotationError<double>(mbc.bodyPosW[bodyIndex_].rotation(),
														targetOri*mbc.bodyPosW[bodyIndex_].rotation(), 1e-7);

	shortJacMat_ = jac_.jacobian(mb, mbc).block(0, 0, 3, shortJacMat_.cols());
	zeroJacobian(shortJacMat_);
	jac_.fullJacobian(mb, shortJacMat_, jacMat_);
}


void OrientationTrackingTask::updateDot(const rbd::MultiBody& mb,
	const rbd::MultiBodyConfig& mbc)
{
	shortJacMat_ = jac_.jacobianDot(mb, mbc).block(0, 0, 3, shortJacMat_.cols());
	zeroJacobian(shortJacMat_);
	jac_.fullJacobian(mb, shortJacMat_, jacDotMat_);
}


const Eigen::MatrixXd& OrientationTrackingTask::jac()
{
	return jacMat_;
}


const Eigen::MatrixXd& OrientationTrackingTask::jacDot()
{
	return jacDotMat_;
}


const Eigen::VectorXd& OrientationTrackingTask::eval()
{
	return eval_;
}


void OrientationTrackingTask::zeroJacobian(Eigen::MatrixXd& jac) const
{
	for(int i: zeroJacIndex_)
	{
		jac.col(i).setZero();
	}
}


} // namespace tasks
