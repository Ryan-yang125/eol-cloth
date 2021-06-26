//
// Created by Apple on 2021/6/27.
//

#ifndef EOL_CLOTH_CONE_H
#define EOL_CLOTH_CONE_H

#include <Eigen/Dense>

#include <vector>
#include <memory>

class Cone
{
public:
    Cone(Eigen::Vector3d _scale, Eigen::Vector3d _position);
    virtual ~Cone() {};

private:
    void createBuffer();

public:
    int point_num;
    int edge_num;

    float* buffer;
    int buffer_size;

    Eigen::Vector3d scale;
    Eigen::Vector3d position;

    Eigen::Matrix<double,3, 6> face_norm;
    Eigen::Matrix<double, 2, 12> edge_face;
    Eigen::Matrix<double, 3, 8> vert_edge;
    Eigen::Matrix<double, 1, 12> edge_direction;

    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};


#endif //EOL_CLOTH_CONE_H
