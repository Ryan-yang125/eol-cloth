#include "obstacle.hpp"

using namespace std;
using namespace Eigen;

Obstacle::Obstacle()
{
	box_num = 0;
	point_cloud = new PointCloud;
	collision_threshold = 1e-2;
}
