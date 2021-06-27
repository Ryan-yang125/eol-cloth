#include "simulator.hpp"
#include "cloth.hpp"
#include "obstacle.hpp"
#include "preprocessor.hpp"
#include "Collisions.hpp"
#include "constraint.hpp"
#include "force.hpp"
#include "external/ArcSim/dynamicremesh.hpp"

using namespace Eigen;
using namespace std;

Simulator::Simulator()
{
	t = 0.0;
	h = 5e-3;
	gravity = Vector3d(0, 0, -9.8);

	Vector2i res = Vector2i(3, 3);
	VectorXd dp00(5), dp01(5), dp10(5), dp11(5);

	dp00 << 0.0, 0.0, 0.0, 0.0, 0.0;
	dp01 << 1.0, 0.0, 0.0, 1.0, 0.0;
	dp10 << 0.0, 1.0, 0.0, 0.0, 1.0;
	dp11 << 1.0, 1.0, 0.0, 1.0, 1.0;

	cloth = new Cloth(res, dp00, dp01, dp10, dp11);
	obstacle = new Obstacle;



    Box* box = new Box(Vector3d(1.2, 1.5, 1.0), Vector3d(0.9175, 0.4425, -0.549));
//    Box* box1 = new Box(Vector3d(0.5, 0.5, 0.5), Vector3d(0.0, 0.0, -0.549));
//    Box* box2 = new Box(Vector3d(0.5, 0.5, 0.5), Vector3d(1.0, 1.0, -0.549));
//    Box* box3 = new Box(Vector3d(0.5, 0.5, 0.5), Vector3d(1.0, 0.0, -0.549));
//    Box* box4 = new Box(Vector3d(0.5, 0.5, 0.5), Vector3d(0.0, 1.0, -0.549));
//
    obstacle->boxes.push_back(box);
//    obstacle->boxes.push_back(box1);
//    obstacle->boxes.push_back(box2);
//    obstacle->boxes.push_back(box3);
//    obstacle->boxes.push_back(box4);
	obstacle->box_num = obstacle->boxes.size();
	

//	obstacle->point_cloud->point_num = 1;
//	obstacle->point_cloud->points_position.resize(3, obstacle->point_cloud->point_num);
//	obstacle->point_cloud->points_normal.resize(3, obstacle->point_cloud->point_num);
//
//	obstacle->point_cloud->points_position.block(0, 0, 3, 1) = Vector3d(0.751,0.751,-0.005);
//	obstacle->point_cloud->points_normal.block(0, 0, 3, 1) = Vector3d(0, -1, 0);

//	obstacle->point_cloud->points_position.block(0, 1, 3, 1) = Vector3d(0.71,0.22,-0.005);
//	obstacle->point_cloud->points_normal.block(0, 1, 3, 1) = Vector3d(0, 0, 1);
//
//	obstacle->point_cloud->points_position.block(0, 2, 3, 1) = Vector3d(0.27, 0.51, -0.005);
//	obstacle->point_cloud->points_normal.block(0, 2, 3, 1) = Vector3d(0, 0, 1);
//
//    obstacle->point_cloud->points_position.block(0, 3, 3, 1) = Vector3d(0.17, 0.31, -0.005);
//    obstacle->point_cloud->points_normal.block(0, 3, 3, 1) = Vector3d(0, 0, 1);

}

void Simulator::step()
{
	cloth->saveOldMesh(); //Just to save old mesh

	Find_Collision(cloth->mesh, obstacle, collisions);

	cloth->preprocessor->preprocess(cloth->mesh, cloth->boundary, collisions);

	dynamic_remesh(cloth->mesh); // external call
	set_indices(cloth->mesh); // external call

	cloth->step(obstacle, gravity, h, t); // key

	collisions.clear(); 

	t += h;
}