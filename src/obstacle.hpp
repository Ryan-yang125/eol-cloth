#ifndef OBSTACLE_H
#define OBSTACLE_H


#include <Eigen/Dense>

#include <vector>
#include <memory>

#include "shapes/Box.hpp"


class PointCloud
{
public:
	PointCloud() { point_num = 0; }
	virtual ~PointCloud() {}

	int point_num;
	Eigen::MatrixXd points_position;
	Eigen::MatrixXd points_normal;

	EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

class Obstacle
{
public:
	Obstacle();
	virtual ~Obstacle() {};

	double collision_threshold;

	int box_num;
	std::vector<Box*> boxes;

	PointCloud* point_cloud;
};

#endif




