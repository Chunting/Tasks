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

#pragma once

// includes
// Eigen
#include <Eigen/Core>

// RBDyn
#include <RBDyn/CoM.h>
#include <RBDyn/Jacobian.h>
#include <RBDyn/MultiBodyConfig.h>
#include <RBDyn/Joint.h>
#include <RBDyn/Momentum.h>

#include "QPContacts.h"
// forward declarations
// RBDyn
namespace rbd
{
	class MultiBody;
	class MultiBodyConfig;
}

namespace tasks
{


class PositionTask
{
public:
	PositionTask(const rbd::MultiBody& mb, int bodyId, const Eigen::Vector3d& pos,
		const Eigen::Vector3d& bodyPoint=Eigen::Vector3d::Zero());

	void position(const Eigen::Vector3d& pos);
	const Eigen::Vector3d& position() const;

	void bodyPoint(const Eigen::Vector3d& point);
	const Eigen::Vector3d& bodyPoint() const;

	void update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);

	const Eigen::VectorXd& eval() const;
	const Eigen::MatrixXd& jac() const;
	const Eigen::MatrixXd& jacDot() const;

private:
	Eigen::Vector3d pos_;
	sva::PTransformd point_;
	int bodyIndex_;
	rbd::Jacobian jac_;

	Eigen::VectorXd eval_;
	Eigen::MatrixXd shortJacMat_;
	Eigen::MatrixXd jacMat_;
	Eigen::MatrixXd jacDotMat_;
};



class OrientationTask
{
public:
	OrientationTask(const rbd::MultiBody& mb, int bodyId, const Eigen::Quaterniond& ori);
	OrientationTask(const rbd::MultiBody& mb, int bodyId, const Eigen::Matrix3d& ori);

	void orientation(const Eigen::Quaterniond& ori);
	void orientation(const Eigen::Matrix3d& ori);
	const Eigen::Matrix3d& orientation() const;

	void update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);

	const Eigen::VectorXd& eval() const;
	const Eigen::MatrixXd& jac() const;
	const Eigen::MatrixXd& jacDot() const;

private:
	Eigen::Matrix3d ori_;
	int bodyIndex_;
	rbd::Jacobian jac_;

	Eigen::VectorXd eval_;
	Eigen::MatrixXd shortJacMat_;
	Eigen::MatrixXd jacMat_;
	Eigen::MatrixXd jacDotMat_;
};



class PostureTask
{
public:
	PostureTask(const rbd::MultiBody& mb, std::vector<std::vector<double> > q);

	void posture(std::vector<std::vector<double> > q);
	const std::vector<std::vector<double> > posture() const;

	void update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);

	const Eigen::VectorXd& eval() const;
	const Eigen::MatrixXd& jac() const;
	const Eigen::MatrixXd& jacDot() const;

private:
	std::vector<std::vector<double> > q_;

	Eigen::VectorXd eval_;
	Eigen::MatrixXd jacMat_;
	Eigen::MatrixXd jacDotMat_;
};



class CoMTask
{
public:
	CoMTask(const rbd::MultiBody& mb, const Eigen::Vector3d& com);

	void com(const Eigen::Vector3d& com);
	const Eigen::Vector3d com() const;

	void update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);

	const Eigen::VectorXd& eval() const;
	const Eigen::MatrixXd& jac() const;
	const Eigen::MatrixXd& jacDot() const;

private:
	Eigen::Vector3d com_;
	rbd::CoMJacobianDummy jac_;

	Eigen::VectorXd eval_;
	Eigen::MatrixXd jacMat_;
	Eigen::MatrixXd jacDotMat_;
};

class ManipCoMTask
{
public:
	ManipCoMTask(const rbd::MultiBody& mb, const Eigen::Vector3d& com,
			const rbd::MultiBody& mbManip, int bodyIdContact,
			const sva::PTransformd& toSurface);

	void com(const Eigen::Vector3d& com);
	const Eigen::Vector3d com() const;

	void update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);

	const Eigen::VectorXd& eval() const;
	const Eigen::MatrixXd& jac() const;
	const Eigen::MatrixXd& jacDot() const;

	const rbd::MultiBody& mbTask() const;
	const rbd::MultiBodyConfig& mbcTask() const;

private:
	Eigen::Vector3d com_;
	rbd::CoMJacobianDummy jac_;

	Eigen::VectorXd eval_;
	Eigen::MatrixXd jacMat_;
	Eigen::MatrixXd jacDotMat_;

	rbd::MultiBody mbManip_;
	rbd::MultiBodyConfig mbcManip_;
};

class MomentumTask
{
public:
	MomentumTask(const rbd::MultiBody& mb, const sva::ForceVecd mom);

	void momentum(const sva::ForceVecd& mom);
	const sva::ForceVec<double> momentum() const;

	void update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateAll(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);

	const Eigen::VectorXd& eval() const;
	const Eigen::MatrixXd& jac() const;
	const Eigen::MatrixXd& jacDot() const;

private:

	sva::ForceVec<double> momentum_;
	rbd::CentroidalMomentumMatrix momentumMatrix_;
	Eigen::VectorXd eval_;
	Eigen::MatrixXd jacMat_;
	Eigen::MatrixXd jacDotMat_;
};

class ManipMomTask
{
public:
	ManipMomTask(const rbd::MultiBody& mb, const sva::ForceVecd& mom,
			const rbd::MultiBody& mbManip, int bodyIdContact,
			const sva::PTransformd& toSurface);

	void momentum(const sva::ForceVecd& mom);
	const sva::ForceVecd momentum() const;

	void update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateAll(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);

	const Eigen::VectorXd& eval() const;
	const Eigen::MatrixXd& jac() const;
	const Eigen::MatrixXd& jacDot() const;

	const rbd::MultiBody& mbTask() const;
	const rbd::MultiBodyConfig& mbcTask() const;

private:
	sva::ForceVecd momentum_;
	rbd::CentroidalMomentumMatrix momentumMatrix_;

	Eigen::VectorXd eval_;
	Eigen::MatrixXd jacMat_;
	Eigen::MatrixXd jacDotMat_;

	rbd::MultiBody mbManip_;
	rbd::MultiBodyConfig mbcManip_;
};

class LinVelocityTask
{
public:
	LinVelocityTask(const rbd::MultiBody& mb, int bodyId, const Eigen::Vector3d& vel,
		const Eigen::Vector3d& bodyPoint=Eigen::Vector3d::Zero());

	void velocity(const Eigen::Vector3d& s);
	const Eigen::Vector3d& velocity() const;

	void bodyPoint(const Eigen::Vector3d& point);
	const Eigen::Vector3d& bodyPoint() const;

	void update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);

	const Eigen::VectorXd& eval() const;
	const Eigen::MatrixXd& jac() const;
	const Eigen::MatrixXd& jacDot() const;

private:
	Eigen::Vector3d vel_;
	sva::PTransformd point_;
	int bodyIndex_;
	rbd::Jacobian jac_;

	Eigen::VectorXd eval_;
	Eigen::MatrixXd shortJacMat_;
	Eigen::MatrixXd jacMat_;
	Eigen::MatrixXd jacDotMat_;
};



class OrientationTrackingTask
{
public:
	OrientationTrackingTask(const rbd::MultiBody& mb, int bodyId,
		const Eigen::Vector3d& bodyPoint, const Eigen::Vector3d& bodyAxis,
		const std::vector<int>& trackingJointsId,
		const Eigen::Vector3d& trackedPoint);

	void trackedPoint(const Eigen::Vector3d& tp);
	const Eigen::Vector3d& trackedPoint() const;

	void bodyPoint(const Eigen::Vector3d& bp);
	const Eigen::Vector3d& bodyPoint() const;

	void bodyAxis(const Eigen::Vector3d& ba);
	const Eigen::Vector3d& bodyAxis() const;

	void update(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);
	void updateDot(const rbd::MultiBody& mb, const rbd::MultiBodyConfig& mbc);

	virtual const Eigen::MatrixXd& jac();
	virtual const Eigen::MatrixXd& jacDot();
	virtual const Eigen::VectorXd& eval();

private:
	void zeroJacobian(Eigen::MatrixXd& jac) const;

private:
	int bodyIndex_;
	sva::PTransformd bodyPoint_;
	Eigen::Vector3d bodyAxis_;
	std::vector<int> zeroJacIndex_;
	Eigen::Vector3d trackedPoint_;
	rbd::Jacobian jac_;

	Eigen::VectorXd eval_;
	Eigen::MatrixXd shortJacMat_;
	Eigen::MatrixXd jacMat_;
	Eigen::MatrixXd jacDotMat_;
};

} // namespace tasks
