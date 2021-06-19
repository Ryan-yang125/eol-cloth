#include "force.h"

void Force::computeForce(const Mesh& mesh, const Material& mat, const Eigen::Vector3d& gravity, double h)
{
	f.resize(mesh.nodes.size() * 3 + mesh.EoL_Count * 2);
	f.setZero();

	std::vector<T> _M;
	std::vector<T> _MDK; //construct Sparse Matrix by triplets


	computeFaceForce(mesh, f, _MDK, _M, gravity, h);

	computeBendingForce(mesh, mat, f, _MDK, h);

	M.resize(mesh.nodes.size() * 3 + mesh.EoL_Count * 2, mesh.nodes.size() * 3 + mesh.EoL_Count * 2);
	K.resize(mesh.nodes.size() * 3 + mesh.EoL_Count * 2, mesh.nodes.size() * 3 + mesh.EoL_Count * 2);

	M.setFromTriplets(_M.begin(), _M.end());
	K.setFromTriplets(_MDK.begin(), _MDK.end());
}

void Force::computeBendingForce(const Mesh& mesh, const Material& mat, Eigen::VectorXd& f, std::vector<T>& _MDK, double h)
{
	for (int e = 0; e < mesh.edges.size(); e++)
	{
		if (mesh.edges[e]->adjf[0] == NULL || mesh.edges[e]->adjf[1] == NULL) continue;
		Edge* current_edge = mesh.edges[e];
		Face* face0 = current_edge->adjf[0];
		Face* face1 = current_edge->adjf[1];
		Node* node0 = current_edge->n[0];
		Node* node1 = current_edge->n[1];
		Vert* vert0 = node0->verts[0];
		Vert* vert1 = node1->verts[0];
		Vert* vert2 = get_other_vert(face0, vert0, vert1);
		Vert* vert3 = get_other_vert(face1, vert0, vert1);
		Node* node2 = vert2->node;
		Node* node3 = vert3->node;

		bool EOLA = node0->EoL;
		bool EOLB = node1->EoL;
		bool EOLC = node2->EoL;
		bool EOLD = node3->EoL;

		int aindex = node0->index * 3;
		int bindex = node1->index * 3;
		int cindex = node2->index * 3;
		int dindex = node3->index * 3;

		int aindeX = mesh.nodes.size() * 3 + node0->EoL_index * 2;
		int bindeX = mesh.nodes.size() * 3 + node1->EoL_index * 2;
		int cindeX = mesh.nodes.size() * 3 + node2->EoL_index * 2;
		int dindeX = mesh.nodes.size() * 3 + node3->EoL_index * 2;

		double W;
		Eigen::VectorXd fb(12), fbe(20);
		Eigen::MatrixXd Kb(12, 12), Kbe(20, 20);

		computeBending(node0->x, node1->x, node2->x, node3->x, vert0->u, vert1->u, vert2->u, vert3->u, mat.beta, W, fb, Kb);

		fbe.segment<12>(0) = fb;
		Kbe.block<12, 12>(0, 0) = Kb;



		if (EOLA || EOLB || EOLC || EOLD)
		{

			computeBendingForceEOL(current_edge, vert0, vert1, vert2, vert3, fbe, Kbe); //final result is here

			f.segment<3>(aindex) += fbe.segment<3>(0);
			if (EOLA) f.segment<2>(aindeX) += fbe.segment<2>(3);

			f.segment<3>(bindex) += fbe.segment<3>(5);
			if (EOLB) f.segment<2>(bindeX) += fbe.segment<2>(8);

			f.segment<3>(cindex) += fbe.segment<3>(10);
			if (EOLC) f.segment<2>(cindeX) += fbe.segment<2>(13);

			f.segment<3>(dindex) += fbe.segment<3>(15);
			if (EOLD) f.segment<2>(dindeX) += fbe.segment<2>(18);

			//distribute the block into global matrix
			Eigen::Matrix3d KLL;
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(0, 0); //aa
			fillLB(_MDK, KLL, aindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(5, 5);//bb
			fillLB(_MDK, KLL, bindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(10, 10);//cc
			fillLB(_MDK, KLL, cindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(15, 15); //dd
			fillLB(_MDK, KLL, dindex);

			KLL = mat.damping2*h*h*Kbe.block<3, 3>(0, 5); //ab
			fillLLB(_MDK, KLL, aindex, bindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(0, 10); //ac
			fillLLB(_MDK, KLL, aindex, cindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(0, 15); //ad
			fillLLB(_MDK, KLL, aindex, dindex);

			KLL = mat.damping2*h*h*Kbe.block<3, 3>(5, 10); //bc
			fillLLB(_MDK, KLL, bindex, cindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(5, 15); //bd
			fillLLB(_MDK, KLL, bindex, dindex);

			KLL = mat.damping2*h*h*Kbe.block<3, 3>(10, 15); //cd
			fillLLB(_MDK, KLL, cindex, dindex);

			Eigen::Matrix2d KEE;
			Eigen::MatrixXd KEL, KLE;
			if (EOLA)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(3, 3);
				fillEB(_MDK, KEE, aindeX);

				KEL = mat.damping2*h*h*Kbe.block<2, 3>(3, 0);
				fillELB(_MDK, KEL, aindeX, aindex);
				KEL = mat.damping2*h*h*Kbe.block<2, 3>(3, 5);
				fillELB(_MDK, KEL, aindeX, bindex);
				KEL = mat.damping2*h*h*Kbe.block<2, 3>(3, 10);
				fillELB(_MDK, KEL, aindeX, cindex);
				KEL = mat.damping2*h*h*Kbe.block<2, 3>(3, 15);
				fillELB(_MDK, KEL, aindeX, dindex);
			}
			if (EOLB)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(8, 8);
				fillEB(_MDK, KEE, bindeX);

				KEL = mat.damping2*h*h*Kbe.block<2, 3>(8, 5);
				fillELB(_MDK, KEL, bindeX, bindex);
				KEL = mat.damping2*h*h*Kbe.block<2, 3>(8, 10);
				fillELB(_MDK, KEL, bindeX, cindex);
				KEL = mat.damping2*h*h*Kbe.block<2, 3>(8, 15);
				fillELB(_MDK, KEL, bindeX, dindex);

				KLE = mat.damping2*h*h*Kbe.block<3, 2>(0, 8);
				fillLEB(_MDK, KLE, aindex, bindeX);
			}
			if (EOLC)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(13, 13);
				fillEB(_MDK, KEE, cindeX);

				KEL = mat.damping2*h*h*Kbe.block<2, 3>(13, 10);
				fillELB(_MDK, KEL, cindeX, cindex);
				KEL = mat.damping2*h*h*Kbe.block<2, 3>(13, 15);
				fillELB(_MDK, KEL, cindeX, dindex);

				KLE = mat.damping2*h*h*Kbe.block<3, 2>(0, 13);
				fillLEB(_MDK, KLE, aindex, cindeX);
				KLE = mat.damping2*h*h*Kbe.block<3, 2>(5, 13);
				fillLEB(_MDK, KLE, bindex, cindeX);
			}

			if (EOLD)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(18, 18);
				fillEB(_MDK, KEE, dindeX);

				KEL = mat.damping2*h*h*Kbe.block<2, 3>(18, 15);
				fillELB(_MDK, KEL, dindeX, dindex);

				KLE = mat.damping2*h*h*Kbe.block<3, 2>(0, 18);
				fillLEB(_MDK, KLE, aindex, dindeX);
				KLE = mat.damping2*h*h*Kbe.block<3, 2>(5, 18);
				fillLEB(_MDK, KLE, bindex, dindeX);
				KLE = mat.damping2*h*h*Kbe.block<3, 2>(10, 18);
				fillLEB(_MDK, KLE, cindex, dindeX);
			}

			if (EOLA && EOLB)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(3, 8);
				fillEEB(_MDK, KEE, aindeX, bindeX);
			}
			if (EOLA && EOLC)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(3, 13);
				fillEEB(_MDK, KEE, aindeX, cindeX);
			}
			if (EOLA && EOLD)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(3, 18);
				fillEEB(_MDK, KEE, aindeX, dindeX);
			}
			if (EOLB && EOLC)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(8, 13);
				fillEEB(_MDK, KEE, bindeX, cindeX);
			}
			if (EOLB && EOLD)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(8, 18);
				fillEEB(_MDK, KEE, bindeX, dindeX);
			}
			if (EOLC && EOLD)
			{
				KEE = mat.damping2*h*h*Kbe.block<2, 2>(13, 18);
				fillEEB(_MDK, KEE, cindeX, dindeX);
			}

		}
		else // not include Eulerian DOF
		{
			f.segment<3>(aindex) += fbe.segment<3>(0);
			f.segment<3>(bindex) += fbe.segment<3>(3);
			f.segment<3>(cindex) += fbe.segment<3>(6);
			f.segment<3>(dindex) += fbe.segment<3>(9);

			Eigen::Matrix3d KLL;
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(0, 0);
			fillLB(_MDK, KLL, aindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(3, 3);
			fillLB(_MDK, KLL, bindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(6, 6);
			fillLB(_MDK, KLL, cindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(9, 9);
			fillLB(_MDK, KLL, dindex);

			KLL = mat.damping2*h*h*Kbe.block<3, 3>(0, 3);
			fillLLB(_MDK, KLL, aindex, bindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(0, 6);
			fillLLB(_MDK, KLL, aindex, cindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(0, 9);
			fillLLB(_MDK, KLL, aindex, dindex);

			KLL = mat.damping2*h*h*Kbe.block<3, 3>(3, 6);
			fillLLB(_MDK, KLL, bindex, cindex);
			KLL = mat.damping2*h*h*Kbe.block<3, 3>(3, 9);
			fillLLB(_MDK, KLL, bindex, dindex);

			KLL = mat.damping2*h*h*Kbe.block<3, 3>(6, 9);
			fillLLB(_MDK, KLL, cindex, dindex);
		}
	}
}


void Force::computeBendingForceEOL(const Edge* edge, const Vert* v0, const Vert* v1, const Vert* v2, const Vert* v3,
	Eigen::VectorXd& fb, Eigen::MatrixXd& Kb)
{
	Eigen::MatrixXd Fa, Fb, Fc, Fd;
	bool EOLA = v0->node->EoL;
	bool EOLB = v1->node->EoL;
	bool EOLC = v2->node->EoL;
	bool EOLD = v3->node->EoL;


	Eigen::MatrixXd F1 = computeF(edge->adjf[0]);
	Eigen::MatrixXd F2 = computeF(edge->adjf[1]);

	if (EOLA) Fa = (F1 + F2) / 2;
	if (EOLB) Fb = (F1 + F2) / 2;
	if (EOLC) Fc = (F1 + F2) / 2;
	if (EOLD) Fd = (F1 + F2) / 2;

	Eigen::Vector3d fba = fb.segment<3>(0);
	Eigen::Vector3d fbb = fb.segment<3>(3);
	Eigen::Vector3d fbc = fb.segment<3>(6);
	Eigen::Vector3d fbd = fb.segment<3>(9);

	Eigen::Matrix3d Kbaa = Kb.block<3, 3>(0, 0);
	Eigen::Matrix3d Kbab = Kb.block<3, 3>(0, 3);
	Eigen::Matrix3d Kbac = Kb.block<3, 3>(0, 6);
	Eigen::Matrix3d Kbad = Kb.block<3, 3>(0, 9);

	Eigen::Matrix3d Kbba = Kb.block<3, 3>(3, 0);
	Eigen::Matrix3d Kbbb = Kb.block<3, 3>(3, 3);
	Eigen::Matrix3d Kbbc = Kb.block<3, 3>(3, 6);
	Eigen::Matrix3d Kbbd = Kb.block<3, 3>(3, 9);

	Eigen::Matrix3d Kbca = Kb.block<3, 3>(6, 0);
	Eigen::Matrix3d Kbcb = Kb.block<3, 3>(6, 3);
	Eigen::Matrix3d Kbcc = Kb.block<3, 3>(6, 6);
	Eigen::Matrix3d Kbcd = Kb.block<3, 3>(6, 9);

	Eigen::Matrix3d Kbda = Kb.block<3, 3>(9, 0);
	Eigen::Matrix3d Kbdb = Kb.block<3, 3>(9, 3);
	Eigen::Matrix3d Kbdc = Kb.block<3, 3>(9, 6);
	Eigen::Matrix3d Kbdd = Kb.block<3, 3>(9, 9); // hessian matrix block

	fb.segment<3>(0) = fba; fb.segment<3>(5) = fbb; fb.segment<3>(10) = fbc; fb.segment<3>(15) = fbd;
	Kb.block<3, 3>(0, 0) = Kbaa; Kb.block<3, 3>(0, 5) = Kbab; Kb.block<3, 3>(0, 10) = Kbac; Kb.block<3, 3>(0, 15) = Kbad;
	Kb.block<3, 3>(5, 5) = Kbbb; Kb.block<3, 3>(5, 10) = Kbbc; Kb.block<3, 3>(5, 15) = Kbbd;
	Kb.block<3, 3>(10, 10) = Kbcc; Kb.block<3, 3>(10, 15) = Kbcd;
	Kb.block<3, 3>(15, 15) = Kbdd;


	if (EOLA) //Fill the "L" area of the stiffness matrix
	{
		fb.segment<2>(3) = -Fa.transpose()*fba;
		Kb.block<2, 2>(3, 3) = Fa.transpose()*Kbaa*Fa;

		Kb.block<2, 3>(3, 0) = -Fa.transpose()*Kbaa;
		Kb.block<2, 3>(3, 5) = -Fa.transpose()*Kbab;
		Kb.block<2, 3>(3, 10) = -Fa.transpose()*Kbac;
		Kb.block<2, 3>(3, 15) = -Fa.transpose()*Kbad;
	}

	if (EOLB)
	{
		fb.segment<2>(8) = -Fb.transpose()*fbb;
		Kb.block<2, 2>(8, 8) = Fb.transpose()*Kbbb*Fb;

		Kb.block<2, 3>(8, 5) = -Fb.transpose()*Kbbb;
		Kb.block<2, 3>(8, 10) = -Fb.transpose()*Kbbc;
		Kb.block<2, 3>(8, 15) = -Fb.transpose()*Kbbd;

		Kb.block<3, 2>(0, 8) = -Kbab * Fb;
	}

	if (EOLC)
	{
		fb.segment<2>(13) = -Fc.transpose()*fbc;
		Kb.block<2, 2>(13, 13) = Fc.transpose()*Kbcc*Fc;

		Kb.block<2, 3>(13, 10) = -Fc.transpose()*Kbcc;
		Kb.block<2, 3>(13, 15) = -Fc.transpose()*Kbcd;

		Kb.block<3, 2>(0, 13) = -Kbac * Fc;
		Kb.block<3, 2>(5, 13) = -Kbbc * Fc;

	}

	if (EOLD)
	{
		fb.segment<2>(18) = -Fd.transpose()*fbd;
		Kb.block<2, 2>(18, 18) = Fd.transpose()*Kbdd*Fd;

		Kb.block<2, 3>(18, 15) = -Fd.transpose()*Kbdd;

		Kb.block<3, 2>(0, 18) = -Kbad * Fd;
		Kb.block<3, 2>(5, 18) = -Kbbd * Fd;
		Kb.block<3, 2>(10, 18) = -Kbcd * Fd;
	}

	if (EOLA && EOLB)
	{
		Kb.block<2, 2>(3, 8) = Fa.transpose()*Kbab*Fb;
	}
	if (EOLA && EOLC)
	{
		Kb.block<2, 2>(3, 13) = Fa.transpose()*Kbac*Fc;
	}
	if (EOLA && EOLD)
	{
		Kb.block<2, 2>(3, 18) = Fa.transpose()*Kbad*Fd;
	}
	if (EOLB && EOLC)
	{
		Kb.block<2, 2>(8, 13) = Fb.transpose()*Kbbc*Fc;
	}
	if (EOLB && EOLD)
	{
		Kb.block<2, 2>(8, 18) = Fb.transpose()*Kbbd*Fd;
	}
	if (EOLC && EOLD)
	{
		Kb.block<2, 2>(13, 18) = Fc.transpose()*Kbcd*Fd;
	}
}


void Force::computeFaceForce(const Mesh& mesh, Eigen::VectorXd& f, std::vector<T>& _MDK, std::vector<T>& _M, const Eigen::Vector3d& gravity, double h)
{
	for (int i = 0; i < mesh.faces.size(); i++)
	{
		Face* current_face = mesh.faces[i];
		const Material* mat = current_face->material;

		Eigen::Vector3d xa = Eigen::Vector3d(current_face->v[0]->node->x[0], current_face->v[0]->node->x[1], current_face->v[0]->node->x[2]);
		Eigen::Vector3d xb = Eigen::Vector3d(current_face->v[1]->node->x[0], current_face->v[1]->node->x[1], current_face->v[1]->node->x[2]);
		Eigen::Vector3d xc = Eigen::Vector3d(current_face->v[2]->node->x[0], current_face->v[2]->node->x[1], current_face->v[2]->node->x[2]);
		Eigen::Vector2d Xa = Eigen::Vector2d(current_face->v[0]->u[0], current_face->v[0]->u[1]);
		Eigen::Vector2d Xb = Eigen::Vector2d(current_face->v[1]->u[0], current_face->v[1]->u[1]);
		Eigen::Vector2d Xc = Eigen::Vector2d(current_face->v[2]->u[0], current_face->v[2]->u[1]);

		Eigen::MatrixXd Dx(3, 2);
		Eigen::MatrixXd DX(2, 2);
		Dx << (xb - xa), (xc - xa);
		DX << (Xb - Xa), (Xc - Xa);

		Eigen::Vector3d norm = (xb - xa).cross(xc - xa);
		Eigen::Vector3d Px = (xb - xa) / (xb - xa).norm();
		Eigen::Vector3d Py = norm.cross(Px);
		Py = Py / Py.norm();
		Eigen::Matrix<double, 2, 3> P;
		P << Px.transpose(), Py.transpose();
		Eigen::Matrix<double, 3, 2> F = Dx * DX.inverse();
		Eigen::Matrix2d Fbar = P * F;
		Eigen::Matrix2d Q = poldec(Fbar);

		int aindex = current_face->v[0]->node->index * 3;
		int bindex = current_face->v[1]->node->index * 3;
		int cindex = current_face->v[2]->node->index * 3;
		int aindeX = mesh.nodes.size() * 3 + current_face->v[0]->node->EoL_index * 2;
		int bindeX = mesh.nodes.size() * 3 + current_face->v[1]->node->EoL_index * 2;
		int cindeX = mesh.nodes.size() * 3 + current_face->v[2]->node->EoL_index * 2;

		Eigen::VectorXd fm(9), fi(9), fme(15), fie(15);
		Eigen::MatrixXd Km(9, 9), Mi(9, 9), Kme(15, 15), Mie(15, 15);
		double Wi, Wm;

		computeMembrane(current_face->v[0]->node->x, current_face->v[1]->node->x, current_face->v[2]->node->x,
			current_face->v[0]->u, current_face->v[1]->u, current_face->v[2]->u, mat->e, mat->nu, P, Q, Wm, fm, Km);
		computeInertial(current_face->v[0]->node->x, current_face->v[1]->node->x, current_face->v[2]->node->x,
			current_face->v[0]->u, current_face->v[1]->u, current_face->v[2]->u, gravity, mat->density, Wi, fi, Mi);

		fme.segment<9>(0) = fm; Kme.block<9, 9>(0, 0) = Km;
		fie.segment<9>(0) = fi; Mie.block<9, 9>(0, 0) = Mi;

		bool EOLA = current_face->v[0]->node->EoL;
		bool EOLB = current_face->v[1]->node->EoL;
		bool EOLC = current_face->v[2]->node->EoL;

		if (EOLA || EOLB || EOLC)
		{
			computeInertialForceEOL(current_face, fie, Mie);
			computeMembraneForceEOL(current_face, fme, Kme);

			f.segment<3>(aindex) += (fme.segment<3>(0) + fie.segment<3>(0));
			if (EOLA) f.segment<2>(aindeX) += (fme.segment<2>(3) + fie.segment<2>(3));

			f.segment<3>(bindex) += (fme.segment<3>(5) + fie.segment<3>(5));
			if (EOLB)f.segment<2>(bindeX) += (fme.segment<2>(8) + fie.segment<2>(8));

			f.segment<3>(cindex) += (fme.segment<3>(10) + fie.segment<3>(10));
			if (EOLC) f.segment<2>(cindeX) += (fme.segment<2>(13) + fie.segment<2>(13));

			Eigen::Matrix3d MLL, KLL;
			KLL = Kme.block<3, 3>(0, 0); MLL = Mie.block<3, 3>(0, 0);
			fillLM(_MDK, _M, KLL, MLL, aindex, mat->damping2, h);
			KLL = Kme.block<3, 3>(5, 5); MLL = Mie.block<3, 3>(5, 5);
			fillLM(_MDK, _M, KLL, MLL, bindex, mat->damping2, h);
			KLL = Kme.block<3, 3>(10, 10); MLL = Mie.block<3, 3>(10, 10);
			fillLM(_MDK, _M, KLL, MLL, cindex, mat->damping2, h);

			KLL = Kme.block<3, 3>(0, 5); MLL = Mie.block<3, 3>(0, 5);
			fillLLM(_MDK, _M, KLL, MLL, aindex, bindex, mat->damping2, h);
			KLL = Kme.block<3, 3>(0, 10); MLL = Mie.block<3, 3>(0, 10);
			fillLLM(_MDK, _M, KLL, MLL, aindex, cindex, mat->damping2, h);
			KLL = Kme.block<3, 3>(5, 10); MLL = Mie.block<3, 3>(5, 10);
			fillLLM(_MDK, _M, KLL, MLL, bindex, cindex, mat->damping2, h);

			Eigen::Matrix2d MEE, KEE;
			Eigen::MatrixXd KEL, KLE, MEL, MLE;
			if (EOLA)
			{
				KEE = Kme.block<2, 2>(3, 3); MEE = Mie.block<2, 2>(3, 3);
				fillEM(_MDK, _M, KEE, MEE, aindeX, mat->damping2, h);

				KEL = Kme.block<2, 3>(3, 0); MEL = Mie.block<2, 3>(3, 0);
				fillELM(_MDK, _M, KEL, MEL, aindeX, aindex, mat->damping2, h);
				KEL = Kme.block<2, 3>(3, 5); MEL = Mie.block<2, 3>(3, 5);
				fillELM(_MDK, _M, KEL, MEL, aindeX, bindex, mat->damping2, h);
				KEL = Kme.block<2, 3>(3, 10); MEL = Mie.block<2, 3>(3, 10);
				fillELM(_MDK, _M, KEL, MEL, aindeX, cindex, mat->damping2, h);
			}
			if (EOLB)
			{
				KEE = Kme.block<2, 2>(8, 8); MEE = Mie.block<2, 2>(8, 8);
				fillEM(_MDK, _M, KEE, MEE, bindeX, mat->damping2, h);

				KEL = Kme.block<2, 3>(8, 5); MEL = Mie.block<2, 3>(8, 5);
				fillELM(_MDK, _M, KEL, MEL, bindeX, bindex, mat->damping2, h);
				KEL = Kme.block<2, 3>(8, 10); MEL = Mie.block<2, 3>(8, 10);
				fillELM(_MDK, _M, KEL, MEL, bindeX, cindex, mat->damping2, h);

				KLE = Kme.block<3, 2>(0, 8); MLE = Mie.block<3, 2>(0, 8);
				fillLEM(_MDK, _M, KLE, MLE, aindex, bindeX, mat->damping2, h);
			}
			if (EOLC)
			{
				KEE = Kme.block<2, 2>(13, 13); MEE = Mie.block<2, 2>(13, 13);
				fillEM(_MDK, _M, KEE, MEE, cindeX, mat->damping2, h);

				KEL = Kme.block<2, 3>(13, 10); MEL = Mie.block<2, 3>(13, 10);
				fillELM(_MDK, _M, KEL, MEL, cindeX, cindex, mat->damping2, h);

				KLE = Kme.block<3, 2>(0, 13); MLE = Mie.block<3, 2>(0, 13);
				fillLEM(_MDK, _M, KLE, MLE, aindex, cindeX, mat->damping2, h);
				KLE = Kme.block<3, 2>(5, 13); MLE = Mie.block<3, 2>(5, 13);
				fillLEM(_MDK, _M, KLE, MLE, bindex, cindeX, mat->damping2, h);

			}

			if (EOLA && EOLB) {
				KEE = Kme.block<2, 2>(3, 8); MEE = Mie.block<2, 2>(3, 8);
				fillEEM(_MDK, _M, KEE, MEE, aindeX, bindeX, mat->damping2, h);
			}
			if (EOLA && EOLC) {
				KEE = Kme.block<2, 2>(3, 13); MEE = Mie.block<2, 2>(3, 13);
				fillEEM(_MDK, _M, KEE, MEE, aindeX, cindeX, mat->damping2, h);
			}
			if (EOLB && EOLC) {
				KEE = Kme.block<2, 2>(8, 13); MEE = Mie.block<2, 2>(8, 13);
				fillEEM(_MDK, _M, KEE, MEE, bindeX, cindeX, mat->damping2, h);
			}
		}
		else
		{
			f.segment<3>(aindex) += fme.segment<3>(0) + fie.segment<3>(0);
			f.segment<3>(bindex) += fme.segment<3>(3) + fie.segment<3>(3);
			f.segment<3>(cindex) += fme.segment<3>(6) + fie.segment<3>(6);

			Eigen::Matrix3d MLL, KLL;
			KLL = Kme.block<3, 3>(0, 0); MLL = Mie.block<3, 3>(0, 0);
			fillLM(_MDK, _M, KLL, MLL, aindex, mat->damping2, h);
			KLL = Kme.block<3, 3>(3, 3); MLL = Mie.block<3, 3>(3, 3);
			fillLM(_MDK, _M, KLL, MLL, bindex, mat->damping2, h);
			KLL = Kme.block<3, 3>(6, 6); MLL = Mie.block<3, 3>(6, 6);
			fillLM(_MDK, _M, KLL, MLL, cindex, mat->damping2, h);

			KLL = Kme.block<3, 3>(0, 3); MLL = Mie.block<3, 3>(0, 3);
			fillLLM(_MDK, _M, KLL, MLL, aindex, bindex, mat->damping2, h);
			KLL = Kme.block<3, 3>(0, 6); MLL = Mie.block<3, 3>(0, 6);
			fillLLM(_MDK, _M, KLL, MLL, aindex, cindex, mat->damping2, h);
			KLL = Kme.block<3, 3>(3, 6); MLL = Mie.block<3, 3>(3, 6);
			fillLLM(_MDK, _M, KLL, MLL, bindex, cindex, mat->damping2, h);
		}
	}
}


void Force::computeMembraneForceEOL(const Face* face, Eigen::VectorXd& fm, Eigen::MatrixXd& Km)
{
	Eigen::MatrixXd Fa, Fb, Fc;
	bool EOLA = face->v[0]->node->EoL;
	bool EOLB = face->v[1]->node->EoL;
	bool EOLC = face->v[2]->node->EoL;
	Eigen::MatrixXd F = computeF(face);
	if (EOLA) Fa = F;
	if (EOLB) Fb = F;
	if (EOLC) Fc = F;

	Eigen::Vector3d fma = fm.segment<3>(0); Eigen::Vector3d fmb = fm.segment<3>(3); Eigen::Vector3d fmc = fm.segment<3>(6);

	Eigen::Matrix3d Kmaa = Km.block<3, 3>(0, 0);
	Eigen::Matrix3d Kmab = Km.block<3, 3>(0, 3);
	Eigen::Matrix3d Kmac = Km.block<3, 3>(0, 6);

	Eigen::Matrix3d Kmba = Km.block<3, 3>(3, 0);
	Eigen::Matrix3d Kmbb = Km.block<3, 3>(3, 3);
	Eigen::Matrix3d Kmbc = Km.block<3, 3>(3, 6);

	Eigen::Matrix3d Kmca = Km.block<3, 3>(6, 0);
	Eigen::Matrix3d Kmcb = Km.block<3, 3>(6, 3);
	Eigen::Matrix3d Kmcc = Km.block<3, 3>(6, 6);

	fm.segment<3>(0) = fma; fm.segment<3>(5) = fmb; fm.segment<3>(10) = fmc;

	Km.block<3, 3>(0, 0) = Kmaa; Km.block<3, 3>(0, 5) = Kmab; Km.block<3, 3>(0, 10) = Kmac;
	Km.block<3, 3>(5, 5) = Kmbb; Km.block<3, 3>(5, 10) = Kmbc;
	Km.block<3, 3>(10, 10) = Kmcc;

	if (EOLA)
	{
		fm.segment<2>(3) = -Fa.transpose() * fma;
		Km.block<2, 2>(3, 3) = Fa.transpose() * Kmaa * Fa;

		Km.block<2, 3>(3, 0) = -Fa.transpose() * Kmaa;
		Km.block<2, 3>(3, 5) = -Fa.transpose() * Kmab;
		Km.block<2, 3>(3, 10) = -Fa.transpose() * Kmac;
	}
	if (EOLB)
	{
		fm.segment<2>(8) = -Fb.transpose() * fmb;
		Km.block<2, 2>(8, 8) = Fb.transpose() * Kmbb * Fb;

		Km.block<2, 3>(8, 5) = -Fb.transpose() * Kmbb;
		Km.block<2, 3>(8, 10) = -Fb.transpose() * Kmbc;

		Km.block<3, 2>(0, 8) = -Kmab * Fb;
	}
	if (EOLC)
	{
		fm.segment<2>(13) = -Fc.transpose() * fmc;
		Km.block<2, 2>(13, 13) = Fc.transpose() * Kmcc * Fc;

		Km.block<2, 3>(13, 10) = -Fc.transpose() * Kmcc;

		Km.block<3, 2>(0, 13) = -Kmac * Fc;
		Km.block<3, 2>(5, 13) = -Kmbc * Fc;
	}

	if (EOLA && EOLB)
	{
		Km.block<2, 2>(3, 8) = Fa.transpose() * Kmab * Fb;
	}
	if (EOLA && EOLC)
	{
		Km.block<2, 2>(3, 13) = Fa.transpose() * Kmac * Fc;
	}
	if (EOLB && EOLC)
	{
		Km.block<2, 2>(8, 13) = Fb.transpose() * Kmbc * Fc;
	}
}

void Force::computeInertialForceEOL(const Face* face, Eigen::VectorXd& fi, Eigen::MatrixXd& Mi)
{
	Eigen::MatrixXd Fa, Fb, Fc;
	bool EOLA = face->v[0]->node->EoL;
	bool EOLB = face->v[1]->node->EoL;
	bool EOLC = face->v[2]->node->EoL;

	Eigen::MatrixXd F = computeF(face);
	if (EOLA) Fa = F;
	if (EOLB) Fb = F;
	if (EOLC) Fc = F;

	Eigen::Vector3d fia = fi.segment<3>(0); Eigen::Vector3d fib = fi.segment<3>(3); Eigen::Vector3d fic = fi.segment<3>(6);

	Eigen::Matrix3d Kiaa = Mi.block<3, 3>(0, 0);
	Eigen::Matrix3d Kiab = Mi.block<3, 3>(0, 3);
	Eigen::Matrix3d Kiac = Mi.block<3, 3>(0, 6);

	Eigen::Matrix3d Kiba = Mi.block<3, 3>(3, 0);
	Eigen::Matrix3d Kibb = Mi.block<3, 3>(3, 3);
	Eigen::Matrix3d Kibc = Mi.block<3, 3>(3, 6);

	Eigen::Matrix3d Kica = Mi.block<3, 3>(6, 0);
	Eigen::Matrix3d Kicb = Mi.block<3, 3>(6, 3);
	Eigen::Matrix3d Kicc = Mi.block<3, 3>(6, 6);

	fi.segment<3>(0) = fia; fi.segment<3>(5) = fib; fi.segment<3>(10) = fic;

	Mi.block<3, 3>(0, 0) = Kiaa; Mi.block<3, 3>(0, 5) = Kiab; Mi.block<3, 3>(0, 10) = Kiac;
	Mi.block<3, 3>(5, 5) = Kibb; Mi.block<3, 3>(5, 10) = Kibc;
	Mi.block<3, 3>(10, 10) = Kicc;

	if (EOLA)
	{
		fi.segment<2>(3) = -Fa.transpose() * fia;
		Mi.block<2, 2>(3, 3) = Fa.transpose() * Kiaa * Fa;

		Mi.block<2, 3>(3, 0) = -Fa.transpose() * Kiaa;
		Mi.block<2, 3>(3, 5) = -Fa.transpose() * Kiab;
		Mi.block<2, 3>(3, 10) = -Fa.transpose() * Kiac;
	}
	if (EOLB)
	{
		fi.segment<2>(8) = -Fb.transpose() * fib;
		Mi.block<2, 2>(8, 8) = Fb.transpose() * Kibb * Fb;

		Mi.block<2, 3>(8, 5) = -Fb.transpose() * Kibb;
		Mi.block<2, 3>(8, 10) = -Fb.transpose() * Kibc;

		Mi.block<3, 2>(0, 8) = -Kiab * Fb;
	}
	if (EOLC) {
		fi.segment<2>(13) = -Fc.transpose() * fic;
		Mi.block<2, 2>(13, 13) = Fc.transpose() * Kicc * Fc;

		Mi.block<2, 3>(13, 10) = -Fc.transpose() * Kicc;

		Mi.block<3, 2>(0, 13) = -Kiac * Fc;
		Mi.block<3, 2>(5, 13) = -Kibc * Fc;
	}

	if (EOLA && EOLB)
	{
		Mi.block<2, 2>(3, 8) = Fa.transpose() * Kiab * Fb;
	}
	if (EOLA && EOLC)
	{
		Mi.block<2, 2>(3, 13) = Fa.transpose() * Kiac * Fc;
	}
	if (EOLB && EOLC)
	{
		Mi.block<2, 2>(8, 13) = Fb.transpose() * Kibc * Fc;
	}
}

void Force::computeBending(const Vec3& x0, const Vec3& x1, const Vec3& x2, const Vec3& x3,
	const Vec3& X0, const Vec3& X1, const Vec3& X2, const Vec3& X3,
	double beta, double& W, Eigen::VectorXd& f, Eigen::MatrixXd& K)
{
	double x0x = x0[0];
	double x0y = x0[1];
	double x0z = x0[2];
	double x1x = x1[0];
	double x1y = x1[1];
	double x1z = x1[2];
	double x2x = x2[0];
	double x2y = x2[1];
	double x2z = x2[2];
	double x3x = x3[0];
	double x3y = x3[1];
	double x3z = x3[2];
	double X0x = X0[0];
	double X0y = X0[1];
	double X1x = X1[0];
	double X1y = X1[1];
	double X2x = X2[0];
	double X2y = X2[1];
	double X3x = X3[0];
	double X3y = X3[1];


	double t2 = pow(X1x - X0x, 2);
	double t4 = pow(X1y - X0y, 2);
	double t6 = beta * (t2 + t4);
	double t17 = 0.1e1 / (-X0x * X2y / 2 + X2x * X0y / 2 + X1x * X2y / 2 - X2x * X1y / 2 + X0x * X3y / 2 - X3x * X0y / 2 - X1x * X3y / 2 + X3x * X1y / 2);
	double t18 = x1y - x0y;
	double t19 = x2z - x0z;
	double t20 = t18 * t19;
	double t21 = x1z - x0z;
	double t22 = x2y - x0y;
	double t23 = t21 * t22;
	double t24 = t20 - t23;
	double t25 = t24 * t24;
	double t26 = x1x - x0x;
	double t27 = t26 * t19;
	double t28 = x2x - x0x;
	double t29 = t21 * t28;
	double t30 = -t27 + t29;
	double t31 = t30 * t30;
	double t32 = t26 * t22;
	double t33 = t18 * t28;
	double t34 = t32 - t33;
	double t35 = t34 * t34;
	double t36 = t25 + t31 + t35;
	double t37 = sqrt(t36);
	double t38 = 0.1e1 / t37;
	double t39 = t38 * t24;
	double t40 = -t18;
	double t41 = x3z - x1z;
	double t42 = t40 * t41;
	double t43 = -t21;
	double t44 = x3y - x1y;
	double t45 = t43 * t44;
	double t46 = t42 - t45;
	double t47 = t46 * t46;
	double t48 = -t26;
	double t49 = t48 * t41;
	double t50 = x3x - x1x;
	double t51 = t43 * t50;
	double t52 = -t49 + t51;
	double t53 = t52 * t52;
	double t54 = t48 * t44;
	double t55 = t40 * t50;
	double t56 = t54 - t55;
	double t57 = t56 * t56;
	double t58 = t47 + t53 + t57;
	double t59 = sqrt(t58);
	double t60 = 0.1e1 / t59;
	double t61 = t60 * t46;
	double t63 = t38 * t30;
	double t64 = t60 * t52;
	double t66 = t38 * t34;
	double t67 = t60 * t56;
	double W00 = 0.3e1 / 0.4e1 * t6 * t17 * (-2 * t39 * t61 - 2 * t63 * t64 - 2 * t66 * t67 + 2);
	double t74 = 0.1e1 / t37 / t36;
	double t75 = t74 * t24;
	double t76 = x2z - x1z;
	double t78 = -x2y + x1y;
	double t81 = 2 * t30 * t76 + 2 * t34 * t78;
	double t82 = t61 * t81;
	double t85 = 0.1e1 / t59 / t58;
	double t86 = t85 * t46;
	double t87 = -t41;
	double t91 = 2 * t56 * t44 + 2 * t52 * t87;
	double t92 = t86 * t91;
	double t94 = t74 * t30;
	double t95 = t64 * t81;
	double t97 = t38 * t76;
	double t100 = t85 * t52;
	double t101 = t100 * t91;
	double t103 = t60 * t87;
	double t106 = t74 * t34;
	double t107 = t67 * t81;
	double t109 = t38 * t78;
	double t112 = t85 * t56;
	double t113 = t112 * t91;
	double t115 = t60 * t44;
	double f01 = -0.3e1 / 0.4e1 * t6 * t17 * (t63 * t101 - 2 * t63 * t103 + t106 * t107 - 2 * t109 * t67 + t66 * t113 - 2 * t66 * t115 + t39 * t92 - 2 * t97 * t64 + t75 * t82 + t94 * t95);
	double t122 = -t76;
	double t124 = -x1x + x2x;
	double t127 = 2 * t24 * t122 + 2 * t34 * t124;
	double t128 = t61 * t127;
	double t130 = t38 * t122;
	double t134 = -t50;
	double t135 = t56 * t134;
	double t137 = 2 * t46 * t41 + 2 * t135;
	double t138 = t86 * t137;
	double t140 = t60 * t41;
	double t143 = t64 * t127;
	double t145 = t100 * t137;
	double t147 = t67 * t127;
	double t149 = t38 * t124;
	double t152 = t112 * t137;
	double t154 = t60 * t134;
	double f02 = -0.3e1 / 0.4e1 * t6 * t17 * (t106 * t147 + t75 * t128 - 2 * t130 * t61 + t39 * t138 - 2 * t39 * t140 + t94 * t143 + t63 * t145 - 2 * t149 * t67 + t66 * t152 - 2 * t66 * t154);
	double t161 = -t78;
	double t163 = -t124;
	double t166 = 2 * t24 * t161 + 2 * t30 * t163;
	double t167 = t61 * t166;
	double t169 = t38 * t161;
	double t172 = -t44;
	double t173 = t46 * t172;
	double t174 = t52 * t50;
	double t176 = 2 * t173 + 2 * t174;
	double t177 = t86 * t176;
	double t179 = t60 * t172;
	double t182 = t64 * t166;
	double t184 = t38 * t163;
	double t187 = t100 * t176;
	double t189 = t60 * t50;
	double t192 = t67 * t166;
	double t194 = t112 * t176;
	double f03 = -0.3e1 / 0.4e1 * t6 * t17 * (t106 * t192 + t75 * t167 - 2 * t169 * t61 + t39 * t177 - 2 * t39 * t179 + t94 * t182 - 2 * t184 * t64 + t63 * t187 - 2 * t63 * t189 + t66 * t194);
	double t200 = -t19;
	double t204 = 2 * t30 * t200 + 2 * t34 * t22;
	double t205 = t61 * t204;
	double t207 = x3z - x0z;
	double t209 = -x3y + x0y;
	double t212 = 2 * t52 * t207 + 2 * t56 * t209;
	double t213 = t86 * t212;
	double t215 = t64 * t204;
	double t217 = t38 * t200;
	double t220 = t100 * t212;
	double t222 = t60 * t207;
	double t225 = t67 * t204;
	double t227 = t38 * t22;
	double t230 = t112 * t212;
	double t232 = t60 * t209;
	double f04 = -0.3e1 / 0.4e1 * t6 * t17 * (t106 * t225 + t75 * t205 + t39 * t213 + t94 * t215 - 2 * t217 * t64 + t63 * t220 - 2 * t63 * t222 - 2 * t227 * t67 + t66 * t230 - 2 * t66 * t232);
	double t240 = -t28;
	double t243 = 2 * t24 * t19 + 2 * t34 * t240;
	double t244 = t61 * t243;
	double t246 = t38 * t19;
	double t249 = -t207;
	double t251 = -x0x + x3x;
	double t252 = t56 * t251;
	double t254 = 2 * t46 * t249 + 2 * t252;
	double t255 = t86 * t254;
	double t257 = t60 * t249;
	double t260 = t64 * t243;
	double t262 = t100 * t254;
	double t264 = t67 * t243;
	double t266 = t38 * t240;
	double t269 = t112 * t254;
	double t271 = t60 * t251;
	double f05 = -0.3e1 / 0.4e1 * t6 * t17 * (t106 * t264 + t75 * t244 - 2 * t246 * t61 + t39 * t255 - 2 * t39 * t257 + t94 * t260 + t63 * t262 - 2 * t266 * t67 + t66 * t269 - 2 * t66 * t271);
	double t278 = -t22;
	double t282 = 2 * t24 * t278 + 2 * t30 * t28;
	double t283 = t61 * t282;
	double t285 = t38 * t278;
	double t288 = -t209;
	double t289 = t46 * t288;
	double t290 = -t251;
	double t291 = t52 * t290;
	double t293 = 2 * t289 + 2 * t291;
	double t294 = t86 * t293;
	double t296 = t60 * t288;
	double t299 = t64 * t282;
	double t301 = t38 * t28;
	double t304 = t100 * t293;
	double t306 = t60 * t290;
	double t309 = t67 * t282;
	double t311 = t112 * t293;
	double f06 = -0.3e1 / 0.4e1 * t6 * t17 * (t106 * t309 + t75 * t283 - 2 * t285 * t61 + t39 * t294 - 2 * t39 * t296 + t94 * t299 - 2 * t301 * t64 + t63 * t304 - 2 * t63 * t306 + t66 * t311);
	double t320 = 2 * t30 * t21 + 2 * t34 * t40;
	double t321 = t61 * t320;
	double t323 = t64 * t320;
	double t325 = t38 * t21;
	double t328 = t67 * t320;
	double t330 = t38 * t40;
	double f07 = -0.3e1 / 0.4e1 * t6 * t17 * (t106 * t328 + t75 * t321 + t94 * t323 - 2 * t325 * t64 - 2 * t330 * t67);
	double t340 = 2 * t24 * t43 + 2 * t34 * t26;
	double t341 = t61 * t340;
	double t343 = t38 * t43;
	double t346 = t64 * t340;
	double t348 = t67 * t340;
	double t350 = t38 * t26;
	double f08 = -0.3e1 / 0.4e1 * t6 * t17 * (t106 * t348 + t75 * t341 - 2 * t343 * t61 + t94 * t346 - 2 * t350 * t67);
	double t360 = 2 * t24 * t18 + 2 * t30 * t48;
	double t361 = t61 * t360;
	double t363 = t38 * t18;
	double t366 = t64 * t360;
	double t368 = t38 * t48;
	double t371 = t67 * t360;
	double f09 = -0.3e1 / 0.4e1 * t6 * t17 * (t106 * t371 + t75 * t361 - 2 * t363 * t61 + t94 * t366 - 2 * t368 * t64);
	double t378 = t56 * t18;
	double t380 = 2 * t52 * t43 + 2 * t378;
	double t381 = t86 * t380;
	double t383 = t100 * t380;
	double t385 = t60 * t43;
	double t388 = t112 * t380;
	double t390 = t60 * t18;
	double f10 = -0.3e1 / 0.4e1 * t6 * t17 * (t39 * t381 + t63 * t383 - 2 * t63 * t385 + t66 * t388 - 2 * t66 * t390);
	double t398 = t56 * t48;
	double t400 = 2 * t46 * t21 + 2 * t398;
	double t401 = t86 * t400;
	double t403 = t60 * t21;
	double t406 = t100 * t400;
	double t408 = t112 * t400;
	double t410 = t60 * t48;
	double f11 = -0.3e1 / 0.4e1 * t6 * t17 * (t39 * t401 - 2 * t39 * t403 + t63 * t406 + t66 * t408 - 2 * t66 * t410);
	double t417 = t46 * t40;
	double t418 = t52 * t26;
	double t420 = 2 * t417 + 2 * t418;
	double t421 = t86 * t420;
	double t423 = t60 * t40;
	double t426 = t100 * t420;
	double t428 = t60 * t26;
	double t431 = t112 * t420;
	double f12 = -0.3e1 / 0.4e1 * t6 * t17 * (t39 * t421 - 2 * t39 * t423 + t63 * t426 - 2 * t63 * t428 + t66 * t431);
	double t437 = t58 * t58;
	double t439 = 0.1e1 / t59 / t437;
	double t440 = t439 * t46;
	double t441 = t91 * t91;
	double t445 = t36 * t36;
	double t447 = 0.1e1 / t37 / t445;
	double t448 = t447 * t30;
	double t449 = t81 * t81;
	double t453 = t439 * t52;
	double t457 = t447 * t34;
	double t461 = t439 * t56;
	double t465 = t447 * t24;
	double t469 = t76 * t76;
	double t470 = t78 * t78;
	double t472 = 2 * t469 + 2 * t470;
	double t475 = t87 * t87;
	double t476 = t44 * t44;
	double t478 = 2 * t475 + 2 * t476;
	double t481 = t74 * t76;
	double t491 = -0.3e1 / 0.2e1 * t39 * t440 * t441 - 0.3e1 / 0.2e1 * t448 * t64 * t449 - 0.3e1 / 0.2e1 * t63 * t453 * t441 - 0.3e1 / 0.2e1 * t457 * t67 * t449 - 0.3e1 / 0.2e1 * t66 * t461 * t441 - 0.3e1 / 0.2e1 * t465 * t61 * t449 + t75 * t61 * t472 + t39 * t86 * t478 + 2 * t481 * t95 + 2 * t94 * t103 * t81 + t94 * t64 * t472 + 2 * t97 * t101;
	double t492 = t85 * t87;
	double t498 = t74 * t78;
	double t508 = t85 * t44;
	double t512 = t94 * t85;
	double t513 = t52 * t81;
	double t516 = t106 * t85;
	double t517 = t56 * t81;
	double t520 = t75 * t85;
	double t521 = t46 * t81;
	double t530 = t63 * t100 * t478 + 2 * t106 * t115 * t81 + t106 * t67 * t472 + t66 * t112 * t478 + 2 * t63 * t492 * t91 + 2 * t66 * t508 * t91 - t512 * t513 * t91 - t516 * t517 * t91 - t520 * t521 * t91 - 4 * t97 * t103 + 2 * t498 * t107 + 2 * t109 * t113 - 4 * t109 * t115;
	double K0101 = 0.3e1 / 0.4e1 * t6 * t17 * (t491 + t530);
	double t534 = t63 * t85;
	double t539 = t457 * t60;
	double t546 = t106 * t60;
	double t551 = t56 * t91;
	double t555 = t66 * t439;
	double t559 = t66 * t85;
	double t563 = t465 * t60;
	double t570 = t75 * t60;
	double t575 = t46 * t91;
	double t579 = t39 * t439;
	double t583 = t39 * t85;
	double t588 = t448 * t60;
	double t595 = t94 * t60;
	double t600 = t52 * t91;
	double t604 = t63 * t439;
	double t608 = 2 * t534 * t52 * t134 * t44 - 0.3e1 / 0.2e1 * t539 * t517 * t127 - t516 * t517 * t137 / 2 + 2 * t546 * t56 * t124 * t78 - t516 * t551 * t127 / 2 - 0.3e1 / 0.2e1 * t555 * t551 * t137 + 2 * t559 * t135 * t44 - 0.3e1 / 0.2e1 * t563 * t521 * t127 - t520 * t521 * t137 / 2 + 2 * t570 * t46 * t124 * t78 - t520 * t575 * t127 / 2 - 0.3e1 / 0.2e1 * t579 * t575 * t137 + 2 * t583 * t46 * t134 * t44 - 0.3e1 / 0.2e1 * t588 * t513 * t127 - t512 * t513 * t137 / 2 + 2 * t595 * t52 * t124 * t78 - t512 * t600 * t127 / 2 - 0.3e1 / 0.2e1 * t604 * t600 * t137;
	double t614 = t74 * t124;
	double t621 = t85 * t134;
	double t628 = t74 * t122;
	double t633 = t85 * t41;
	double t641 = t97 * t145 + t94 * t103 * t127 + t63 * t492 * t137 + t614 * t107 + t106 * t154 * t81 + t498 * t147 + t109 * t152 + t149 * t113 + t66 * t621 * t91 + t106 * t115 * t127 + t66 * t508 * t137 + t628 * t82 + t75 * t140 * t81 + t130 * t92 + t39 * t633 * t91 + t481 * t143 - 2 * t109 * t154 - 2 * t149 * t115;
	double K0102 = 0.3e1 / 0.4e1 * t6 * t17 * (t608 + t641);
	double t704 = -0.3e1 / 0.2e1 * t563 * t521 * t166 - t520 * t521 * t176 / 2 + 2 * t570 * t46 * t163 * t76 - t520 * t575 * t166 / 2 - 0.3e1 / 0.2e1 * t579 * t575 * t176 + 2 * t583 * t46 * t50 * t87 - 0.3e1 / 0.2e1 * t588 * t513 * t166 - t512 * t513 * t176 / 2 + 2 * t595 * t52 * t163 * t76 - t512 * t600 * t166 / 2 - 0.3e1 / 0.2e1 * t604 * t600 * t176 + 2 * t534 * t174 * t87 - 0.3e1 / 0.2e1 * t539 * t517 * t166 - t516 * t517 * t176 / 2 + 2 * t546 * t56 * t163 * t76 - t516 * t551 * t166 / 2 - 0.3e1 / 0.2e1 * t555 * t551 * t176 + 2 * t559 * t56 * t50 * t87;
	double t705 = t74 * t161;
	double t710 = t85 * t172;
	double t713 = t74 * t163;
	double t720 = t85 * t50;
	double t737 = t705 * t82 + t75 * t179 * t81 + t169 * t92 + t39 * t710 * t91 + t713 * t95 + t94 * t189 * t81 + t481 * t182 + t97 * t187 + t184 * t101 + t63 * t720 * t91 + t94 * t103 * t166 + t63 * t492 * t176 + t498 * t192 + t109 * t194 + t106 * t115 * t166 + t66 * t508 * t176 - 2 * t97 * t189 - 2 * t184 * t103;
	double K0103 = 0.3e1 / 0.4e1 * t6 * t17 * (t704 + t737);
	double t778 = t85 * t209;
	double t784 = 2 * t207 * t87 + 2 * t209 * t44;
	double t794 = -t512 * t600 * t204 / 2 - 0.3e1 / 0.2e1 * t604 * t600 * t212 - 0.3e1 / 0.2e1 * t539 * t517 * t204 - t516 * t517 * t212 / 2 - t516 * t551 * t204 / 2 - 0.3e1 / 0.2e1 * t555 * t551 * t212 - 0.3e1 / 0.2e1 * t563 * t521 * t204 - t520 * t521 * t212 / 2 - t520 * t575 * t204 / 2 - 0.3e1 / 0.2e1 * t579 * t575 * t212 - 0.3e1 / 0.2e1 * t588 * t513 * t204 - t512 * t513 * t212 / 2 + t227 * t113 + t66 * t778 * t91 + t66 * t112 * t784 + t106 * t115 * t204 + t66 * t508 * t212 + t109 * t230 + t39 * t86 * t784;
	double t795 = t74 * t200;
	double t802 = 2 * t200 * t76 + 2 * t22 * t78;
	double t808 = t85 * t207;
	double t817 = t74 * t22;
	double t834 = t795 * t95 + t94 * t222 * t81 + t94 * t64 * t802 + t481 * t215 + t97 * t220 + t217 * t101 + t63 * t808 * t91 + t63 * t100 * t784 + t94 * t103 * t204 + t63 * t492 * t212 + t817 * t107 + t106 * t232 * t81 + t106 * t67 * t802 + t498 * t225 + t75 * t61 * t802 - 2 * t97 * t222 - 2 * t217 * t103 - 2 * t109 * t232 - 2 * t227 * t115;
	double K0104 = 0.3e1 / 0.4e1 * t6 * t17 * (t794 + t834);
	double t839 = 2 * t66 * t60;
	double t840 = t38 * t60;
	double t842 = 2 * t840 * t56;
	double t881 = t74 * t240;
	double t887 = 2 * t240 * t78 + 2 * t32 - 2 * t33;
	double t891 = t839 - t842 - 0.3e1 / 0.2e1 * t563 * t521 * t243 - t520 * t521 * t254 / 2 - t520 * t575 * t243 / 2 - 0.3e1 / 0.2e1 * t579 * t575 * t254 - 0.3e1 / 0.2e1 * t588 * t513 * t243 - t512 * t513 * t254 / 2 - t512 * t600 * t243 / 2 - 0.3e1 / 0.2e1 * t604 * t600 * t254 - 0.3e1 / 0.2e1 * t539 * t517 * t243 - t516 * t517 * t254 / 2 - t516 * t551 * t243 / 2 - 0.3e1 / 0.2e1 * t555 * t551 * t254 + t63 * t492 * t254 + t881 * t107 + t106 * t271 * t81 + t106 * t67 * t887 + t498 * t264;
	double t894 = t85 * t251;
	double t899 = 2 * t251 * t44 - 2 * t54 + 2 * t55;
	double t906 = t74 * t19;
	double t913 = t85 * t249;
	double t930 = t109 * t269 + t266 * t113 + t66 * t894 * t91 + t66 * t112 * t899 + t106 * t115 * t243 + t66 * t508 * t254 + t906 * t82 + t75 * t257 * t81 + t75 * t61 * t887 + t246 * t92 + t39 * t913 * t91 + t39 * t86 * t899 + t94 * t64 * t887 + t481 * t260 + t97 * t262 + t63 * t100 * t899 + t94 * t103 * t243 - 2 * t109 * t271 - 2 * t266 * t115;
	double K0105 = 0.3e1 / 0.4e1 * t6 * t17 * (t891 + t930);
	double t935 = 2 * t63 * t60;
	double t937 = 2 * t840 * t52;
	double t974 = t85 * t288;
	double t979 = 2 * t290 * t87 - 2 * t49 + 2 * t51;
	double t982 = t74 * t28;
	double t988 = 2 * t28 * t76 + 2 * t27 - 2 * t29;
	double t991 = -t935 + t937 - 0.3e1 / 0.2e1 * t563 * t521 * t282 - t520 * t521 * t293 / 2 - t520 * t575 * t282 / 2 - 0.3e1 / 0.2e1 * t579 * t575 * t293 - 0.3e1 / 0.2e1 * t588 * t513 * t282 - t512 * t513 * t293 / 2 - t512 * t600 * t282 / 2 - 0.3e1 / 0.2e1 * t604 * t600 * t293 - 0.3e1 / 0.2e1 * t539 * t517 * t282 - t516 * t517 * t293 / 2 - t516 * t551 * t282 / 2 - 0.3e1 / 0.2e1 * t555 * t551 * t293 + t39 * t974 * t91 + t39 * t86 * t979 + t982 * t95 + t94 * t306 * t81 + t94 * t64 * t988;
	double t995 = t85 * t290;
	double t1014 = t74 * t278;
	double t1025 = t481 * t299 + t97 * t304 + t301 * t101 + t63 * t995 * t91 + t63 * t100 * t979 + t94 * t103 * t282 + t63 * t492 * t293 + t106 * t67 * t988 + t498 * t309 + t109 * t311 + t66 * t112 * t979 + t106 * t115 * t282 + t66 * t508 * t293 + t1014 * t82 + t75 * t296 * t81 + t75 * t61 * t988 + t285 * t92 - 2 * t97 * t306 - 2 * t301 * t103;
	double K0106 = 0.3e1 / 0.4e1 * t6 * t17 * (t991 + t1025);
	double t1035 = 2 * t21 * t76 + 2 * t40 * t78;
	double t1044 = t74 * t21;
	double t1060 = t74 * t40;
	double t1073 = -0.3e1 / 0.2e1 * t563 * t521 * t320 + t75 * t61 * t1035 - t520 * t575 * t320 / 2 - 0.3e1 / 0.2e1 * t588 * t513 * t320 + t1044 * t95 + t94 * t64 * t1035 + t481 * t323 - t512 * t600 * t320 / 2 + t325 * t101 + t94 * t103 * t320 - 2 * t325 * t103 - 0.3e1 / 0.2e1 * t539 * t517 * t320 + t1060 * t107 + t106 * t67 * t1035 + t498 * t328 - t516 * t551 * t320 / 2 + t330 * t113 + t106 * t115 * t320 - 2 * t330 * t115;
	double K0107 = 0.3e1 / 0.4e1 * t6 * t17 * t1073;
	double t1079 = t74 * t43;
	double t1083 = 2 * t26 * t78 - 2 * t32 + 2 * t33;
	double t1104 = t74 * t26;
	double t1117 = -0.3e1 / 0.2e1 * t563 * t521 * t340 + t1079 * t82 + t75 * t61 * t1083 - t520 * t575 * t340 / 2 + t343 * t92 - 0.3e1 / 0.2e1 * t588 * t513 * t340 + t94 * t64 * t1083 + t481 * t346 - t512 * t600 * t340 / 2 + t94 * t103 * t340 - 0.3e1 / 0.2e1 * t539 * t517 * t340 + t1104 * t107 + t106 * t67 * t1083 + t498 * t348 + t842 - t516 * t551 * t340 / 2 + t350 * t113 + t106 * t115 * t340 - 2 * t350 * t115;
	double K0108 = 0.3e1 / 0.4e1 * t6 * t17 * t1117;
	double t1123 = t74 * t18;
	double t1127 = 2 * t48 * t76 - 2 * t27 + 2 * t29;
	double t1137 = t74 * t48;
	double t1161 = -0.3e1 / 0.2e1 * t563 * t521 * t360 + t1123 * t82 + t75 * t61 * t1127 - t520 * t575 * t360 / 2 + t363 * t92 - 0.3e1 / 0.2e1 * t588 * t513 * t360 + t1137 * t95 + t94 * t64 * t1127 + t481 * t366 - t937 - t512 * t600 * t360 / 2 + t368 * t101 + t94 * t103 * t360 - 2 * t368 * t103 - 0.3e1 / 0.2e1 * t539 * t517 * t360 + t106 * t67 * t1127 + t498 * t371 - t516 * t551 * t360 / 2 + t106 * t115 * t360;
	double K0109 = 0.3e1 / 0.4e1 * t6 * t17 * t1161;
	double t1173 = 2 * t18 * t44 + 2 * t43 * t87;
	double t1187 = t85 * t43;
	double t1205 = t85 * t18;
	double t1212 = -t520 * t521 * t380 / 2 - 0.3e1 / 0.2e1 * t579 * t575 * t380 + t39 * t86 * t1173 - t512 * t513 * t380 / 2 + t94 * t385 * t81 + t97 * t383 - 2 * t97 * t385 - 0.3e1 / 0.2e1 * t604 * t600 * t380 + t63 * t1187 * t91 + t63 * t100 * t1173 + t63 * t492 * t380 - t516 * t517 * t380 / 2 + t106 * t390 * t81 + t109 * t388 - 2 * t109 * t390 - 0.3e1 / 0.2e1 * t555 * t551 * t380 + t66 * t1205 * t91 + t66 * t112 * t1173 + t66 * t508 * t380;
	double K0110 = 0.3e1 / 0.4e1 * t6 * t17 * t1212;
	double t1223 = t85 * t21;
	double t1228 = 4 * t54 - 2 * t55;
	double t1253 = t85 * t48;
	double t1260 = -t520 * t521 * t400 / 2 + t75 * t403 * t81 - 0.3e1 / 0.2e1 * t579 * t575 * t400 + t39 * t1223 * t91 + t39 * t86 * t1228 - t512 * t513 * t400 / 2 + t97 * t406 - 0.3e1 / 0.2e1 * t604 * t600 * t400 + t63 * t100 * t1228 + t63 * t492 * t400 - t516 * t517 * t400 / 2 + t106 * t410 * t81 + t109 * t408 - 2 * t109 * t410 - 0.3e1 / 0.2e1 * t555 * t551 * t400 + t66 * t1253 * t91 + t66 * t112 * t1228 + t66 * t508 * t400 - t839;
	double K0111 = 0.3e1 / 0.4e1 * t6 * t17 * t1260;
	double t1271 = t85 * t40;
	double t1276 = 2 * t26 * t87 + 2 * t49 - 2 * t51;
	double t1290 = t85 * t26;
	double t1308 = -t520 * t521 * t420 / 2 + t75 * t423 * t81 - 0.3e1 / 0.2e1 * t579 * t575 * t420 + t39 * t1271 * t91 + t39 * t86 * t1276 - t512 * t513 * t420 / 2 + t94 * t428 * t81 + t97 * t426 - 2 * t97 * t428 - 0.3e1 / 0.2e1 * t604 * t600 * t420 + t63 * t1290 * t91 + t63 * t100 * t1276 + t63 * t492 * t420 + t935 - t516 * t517 * t420 / 2 + t109 * t431 - 0.3e1 / 0.2e1 * t555 * t551 * t420 + t66 * t112 * t1276 + t66 * t508 * t420;
	double K0112 = 0.3e1 / 0.4e1 * t6 * t17 * t1308;
	double K0201 = K0102;
	double t1315 = t127 * t127;
	double t1319 = t137 * t137;
	double t1335 = t122 * t122;
	double t1336 = t124 * t124;
	double t1338 = 2 * t1335 + 2 * t1336;
	double t1346 = t41 * t41;
	double t1347 = t134 * t134;
	double t1349 = 2 * t1346 + 2 * t1347;
	double t1352 = -4 * t130 * t140 - 4 * t149 * t154 - 0.3e1 / 0.2e1 * t465 * t61 * t1315 - 0.3e1 / 0.2e1 * t39 * t440 * t1319 - 0.3e1 / 0.2e1 * t448 * t64 * t1315 - 0.3e1 / 0.2e1 * t63 * t453 * t1319 - 0.3e1 / 0.2e1 * t457 * t67 * t1315 - 0.3e1 / 0.2e1 * t66 * t461 * t1319 + t106 * t67 * t1338 + 2 * t149 * t152 + 2 * t66 * t621 * t137 + t66 * t112 * t1349;
	double t1376 = t46 * t127;
	double t1379 = t52 * t127;
	double t1382 = t56 * t127;
	double t1385 = t63 * t100 * t1349 + 2 * t106 * t154 * t127 + 2 * t75 * t140 * t127 + t75 * t61 * t1338 + t94 * t64 * t1338 + t39 * t86 * t1349 - t520 * t1376 * t137 - t512 * t1379 * t137 - t516 * t1382 * t137 + 2 * t39 * t633 * t137 + 2 * t628 * t128 + 2 * t130 * t138 + 2 * t614 * t147;
	double K0202 = 0.3e1 / 0.4e1 * t6 * t17 * (t1352 + t1385);
	double t1393 = t52 * t137;
	double t1426 = -2 * t130 * t179 - 2 * t169 * t140 - t512 * t1393 * t166 / 2 - 0.3e1 / 0.2e1 * t604 * t1393 * t176 + 2 * t534 * t52 * t172 * t41 - 0.3e1 / 0.2e1 * t539 * t1382 * t166 - t516 * t1382 * t176 / 2 + t628 * t167 + t130 * t177 + t169 * t138 + t39 * t710 * t137 + t75 * t140 * t166 + t39 * t633 * t176 + t713 * t143 + t94 * t189 * t127 + t184 * t145 + t63 * t720 * t137 + t614 * t192;
	double t1439 = t56 * t137;
	double t1460 = t46 * t137;
	double t1480 = t149 * t194 + t106 * t154 * t166 + t66 * t621 * t176 + t705 * t128 + t75 * t179 * t127 + 2 * t546 * t56 * t161 * t122 - t516 * t1439 * t166 / 2 - 0.3e1 / 0.2e1 * t555 * t1439 * t176 + 2 * t559 * t56 * t172 * t41 - 0.3e1 / 0.2e1 * t563 * t1376 * t166 - t520 * t1376 * t176 / 2 + 2 * t570 * t46 * t161 * t122 - t520 * t1460 * t166 / 2 - 0.3e1 / 0.2e1 * t579 * t1460 * t176 + 2 * t583 * t173 * t41 - 0.3e1 / 0.2e1 * t588 * t1379 * t166 - t512 * t1379 * t176 / 2 + 2 * t595 * t52 * t161 * t122;
	double K0203 = 0.3e1 / 0.4e1 * t6 * t17 * (t1426 + t1480);
	double t1530 = 2 * t22 * t124 - 2 * t32 + 2 * t33;
	double t1535 = -2 * t149 * t232 - 2 * t227 * t154 - 0.3e1 / 0.2e1 * t563 * t1376 * t204 - t520 * t1376 * t212 / 2 - t520 * t1460 * t204 / 2 - 0.3e1 / 0.2e1 * t579 * t1460 * t212 - 0.3e1 / 0.2e1 * t588 * t1379 * t204 - t512 * t1379 * t212 / 2 - t512 * t1393 * t204 / 2 - 0.3e1 / 0.2e1 * t604 * t1393 * t212 - 0.3e1 / 0.2e1 * t539 * t1382 * t204 - t516 * t1382 * t212 / 2 - t516 * t1439 * t204 / 2 - 0.3e1 / 0.2e1 * t555 * t1439 * t212 + t106 * t154 * t204 + t66 * t621 * t212 + t75 * t61 * t1530 + t628 * t205 + t130 * t213;
	double t1538 = 2 * t209 * t134 + 2 * t54 - 2 * t55;
	double t1567 = t39 * t86 * t1538 + t75 * t140 * t204 + t39 * t633 * t212 + t795 * t143 + t94 * t222 * t127 + t94 * t64 * t1530 + t217 * t145 + t63 * t808 * t137 + t63 * t100 * t1538 + t817 * t147 + t106 * t232 * t127 + t106 * t67 * t1530 + t614 * t225 + t149 * t230 + t227 * t152 + t66 * t778 * t137 + t66 * t112 * t1538 - t839 + t842;
	double K0204 = 0.3e1 / 0.4e1 * t6 * t17 * (t1535 + t1567);
	double t1582 = 2 * t251 * t134 + 2 * t249 * t41;
	double t1625 = -2 * t130 * t257 - 2 * t246 * t140 - 2 * t149 * t271 - 2 * t266 * t154 + t66 * t112 * t1582 + t106 * t154 * t243 + t66 * t621 * t254 - 0.3e1 / 0.2e1 * t563 * t1376 * t243 - t520 * t1376 * t254 / 2 - t520 * t1460 * t243 / 2 - 0.3e1 / 0.2e1 * t579 * t1460 * t254 - 0.3e1 / 0.2e1 * t588 * t1379 * t243 - t512 * t1379 * t254 / 2 - t512 * t1393 * t243 / 2 - 0.3e1 / 0.2e1 * t604 * t1393 * t254 - 0.3e1 / 0.2e1 * t539 * t1382 * t243 - t516 * t1382 * t254 / 2 - t516 * t1439 * t243 / 2 - 0.3e1 / 0.2e1 * t555 * t1439 * t254;
	double t1632 = 2 * t19 * t122 + 2 * t240 * t124;
	double t1660 = t906 * t128 + t75 * t257 * t127 + t75 * t61 * t1632 + t628 * t244 + t130 * t255 + t246 * t138 + t39 * t913 * t137 + t39 * t86 * t1582 + t75 * t140 * t243 + t39 * t633 * t254 + t94 * t64 * t1632 + t63 * t100 * t1582 + t881 * t147 + t106 * t271 * t127 + t106 * t67 * t1632 + t614 * t264 + t149 * t269 + t266 * t152 + t66 * t894 * t137;
	double K0205 = 0.3e1 / 0.4e1 * t6 * t17 * (t1625 + t1660);
	double t1681 = 2 * t288 * t41 - 2 * t42 + 2 * t45;
	double t1693 = 2 * t278 * t122 + 2 * t20 - 2 * t23;
	double t1704 = -2 * t130 * t296 - 2 * t285 * t140 - 0.3e1 / 0.2e1 * t604 * t1393 * t293 - 0.3e1 / 0.2e1 * t539 * t1382 * t282 + t628 * t283 + t130 * t294 + t285 * t138 + t39 * t974 * t137 + t39 * t86 * t1681 + t75 * t140 * t282 + t39 * t633 * t293 + t982 * t143 + t94 * t306 * t127 + t94 * t64 * t1693 + t301 * t145 + t63 * t995 * t137 + t63 * t100 * t1681 + t106 * t67 * t1693 + t614 * t309;
	double t1748 = 2 * t840 * t46;
	double t1750 = 2 * t39 * t60;
	double t1751 = t149 * t311 + t66 * t112 * t1681 + t106 * t154 * t282 + t66 * t621 * t293 + t1014 * t128 + t75 * t296 * t127 + t75 * t61 * t1693 - t516 * t1382 * t293 / 2 - t516 * t1439 * t282 / 2 - 0.3e1 / 0.2e1 * t555 * t1439 * t293 - 0.3e1 / 0.2e1 * t563 * t1376 * t282 - t520 * t1376 * t293 / 2 - t520 * t1460 * t282 / 2 - 0.3e1 / 0.2e1 * t579 * t1460 * t293 - 0.3e1 / 0.2e1 * t588 * t1379 * t282 - t512 * t1379 * t293 / 2 - t512 * t1393 * t282 / 2 - t1748 + t1750;
	double K0206 = 0.3e1 / 0.4e1 * t6 * t17 * (t1704 + t1751);
	double t1760 = 2 * t40 * t124 + 2 * t32 - 2 * t33;
	double t1794 = -0.3e1 / 0.2e1 * t563 * t1376 * t320 + t75 * t61 * t1760 + t628 * t321 - t520 * t1460 * t320 / 2 + t75 * t140 * t320 - 0.3e1 / 0.2e1 * t588 * t1379 * t320 + t1044 * t143 + t94 * t64 * t1760 - t512 * t1393 * t320 / 2 + t325 * t145 - 0.3e1 / 0.2e1 * t539 * t1382 * t320 + t1060 * t147 + t106 * t67 * t1760 + t614 * t328 - t842 - t516 * t1439 * t320 / 2 + t330 * t152 + t106 * t154 * t320 - 2 * t330 * t154;
	double K0207 = 0.3e1 / 0.4e1 * t6 * t17 * t1794;
	double t1804 = 2 * t43 * t122 + 2 * t26 * t124;
	double t1839 = -0.3e1 / 0.2e1 * t563 * t1376 * t340 + t1079 * t128 + t75 * t61 * t1804 + t628 * t341 - t520 * t1460 * t340 / 2 + t343 * t138 + t75 * t140 * t340 - 2 * t343 * t140 - 0.3e1 / 0.2e1 * t588 * t1379 * t340 + t94 * t64 * t1804 - t512 * t1393 * t340 / 2 - 0.3e1 / 0.2e1 * t539 * t1382 * t340 + t1104 * t147 + t106 * t67 * t1804 + t614 * t348 - t516 * t1439 * t340 / 2 + t350 * t152 + t106 * t154 * t340 - 2 * t350 * t154;
	double K0208 = 0.3e1 / 0.4e1 * t6 * t17 * t1839;
	double t1848 = 2 * t18 * t122 - 2 * t20 + 2 * t23;
	double t1881 = -0.3e1 / 0.2e1 * t563 * t1376 * t360 + t1123 * t128 + t75 * t61 * t1848 + t628 * t361 + t1748 - t520 * t1460 * t360 / 2 + t363 * t138 + t75 * t140 * t360 - 2 * t363 * t140 - 0.3e1 / 0.2e1 * t588 * t1379 * t360 + t1137 * t143 + t94 * t64 * t1848 - t512 * t1393 * t360 / 2 + t368 * t145 - 0.3e1 / 0.2e1 * t539 * t1382 * t360 + t106 * t67 * t1848 + t614 * t371 - t516 * t1439 * t360 / 2 + t106 * t154 * t360;
	double K0209 = 0.3e1 / 0.4e1 * t6 * t17 * t1881;
	double t1893 = 2 * t18 * t134 - 2 * t54 + 2 * t55;
	double t1927 = -t520 * t1376 * t380 / 2 + t130 * t381 - 0.3e1 / 0.2e1 * t579 * t1460 * t380 + t39 * t86 * t1893 + t39 * t633 * t380 - t512 * t1379 * t380 / 2 + t94 * t385 * t127 - 0.3e1 / 0.2e1 * t604 * t1393 * t380 + t63 * t1187 * t137 + t63 * t100 * t1893 - t516 * t1382 * t380 / 2 + t106 * t390 * t127 + t149 * t388 - 2 * t149 * t390 - 0.3e1 / 0.2e1 * t555 * t1439 * t380 + t66 * t1205 * t137 + t66 * t112 * t1893 + t66 * t621 * t380 + t839;
	double K0210 = 0.3e1 / 0.4e1 * t6 * t17 * t1927;
	double t1946 = 2 * t48 * t134 + 2 * t21 * t41;
	double t1976 = -t520 * t1376 * t400 / 2 + t75 * t403 * t127 + t130 * t401 - 2 * t130 * t403 - 0.3e1 / 0.2e1 * t579 * t1460 * t400 + t39 * t1223 * t137 + t39 * t86 * t1946 + t39 * t633 * t400 - t512 * t1379 * t400 / 2 - 0.3e1 / 0.2e1 * t604 * t1393 * t400 + t63 * t100 * t1946 - t516 * t1382 * t400 / 2 + t106 * t410 * t127 + t149 * t408 - 2 * t149 * t410 - 0.3e1 / 0.2e1 * t555 * t1439 * t400 + t66 * t1253 * t137 + t66 * t112 * t1946 + t66 * t621 * t400;
	double K0211 = 0.3e1 / 0.4e1 * t6 * t17 * t1976;
	double t1994 = 4 * t42 - 2 * t45;
	double t2022 = -t520 * t1376 * t420 / 2 + t75 * t423 * t127 + t130 * t421 - 2 * t130 * t423 - 0.3e1 / 0.2e1 * t579 * t1460 * t420 + t39 * t1271 * t137 + t39 * t86 * t1994 + t39 * t633 * t420 - t1750 - t512 * t1379 * t420 / 2 + t94 * t428 * t127 - 0.3e1 / 0.2e1 * t604 * t1393 * t420 + t63 * t1290 * t137 + t63 * t100 * t1994 - t516 * t1382 * t420 / 2 + t149 * t431 - 0.3e1 / 0.2e1 * t555 * t1439 * t420 + t66 * t112 * t1994 + t66 * t621 * t420;
	double K0212 = 0.3e1 / 0.4e1 * t6 * t17 * t2022;
	double K0301 = K0103;
	double K0302 = K0203;
	double t2029 = t166 * t166;
	double t2033 = t176 * t176;
	double t2054 = t161 * t161;
	double t2055 = t163 * t163;
	double t2057 = 2 * t2054 + 2 * t2055;
	double t2062 = -4 * t169 * t179 - 4 * t184 * t189 - 0.3e1 / 0.2e1 * t465 * t61 * t2029 - 0.3e1 / 0.2e1 * t39 * t440 * t2033 - 0.3e1 / 0.2e1 * t448 * t64 * t2029 - 0.3e1 / 0.2e1 * t63 * t453 * t2033 - 0.3e1 / 0.2e1 * t457 * t67 * t2029 - 0.3e1 / 0.2e1 * t66 * t461 * t2033 + 2 * t705 * t167 + 2 * t75 * t179 * t166 + t75 * t61 * t2057 + 2 * t169 * t177;
	double t2066 = t172 * t172;
	double t2067 = t50 * t50;
	double t2069 = 2 * t2066 + 2 * t2067;
	double t2090 = t46 * t166;
	double t2093 = t52 * t166;
	double t2096 = t56 * t166;
	double t2099 = t63 * t100 * t2069 + t106 * t67 * t2057 + t66 * t112 * t2069 + 2 * t94 * t189 * t166 - t520 * t2090 * t176 - t512 * t2093 * t176 - t516 * t2096 * t176 + 2 * t39 * t710 * t176 + 2 * t63 * t720 * t176 + t94 * t64 * t2057 + t39 * t86 * t2069 + 2 * t713 * t182 + 2 * t184 * t187;
	double K0303 = 0.3e1 / 0.4e1 * t6 * t17 * (t2062 + t2099);
	double t2112 = 2 * t207 * t50 + 2 * t49 - 2 * t51;
	double t2117 = 2 * t200 * t163 - 2 * t27 + 2 * t29;
	double t2140 = -2 * t184 * t222 - 2 * t217 * t189 + t227 * t194 + t66 * t778 * t176 + t66 * t112 * t2112 + t75 * t61 * t2117 + t705 * t205 + t169 * t213 + t39 * t86 * t2112 + t75 * t179 * t204 + t39 * t710 * t212 + t795 * t182 + t94 * t222 * t166 + t94 * t64 * t2117 + t713 * t215 + t184 * t220 + t217 * t187 + t63 * t808 * t176 + t63 * t100 * t2112;
	double t2156 = t46 * t176;
	double t2169 = t52 * t176;
	double t2182 = t56 * t176;
	double t2189 = t94 * t189 * t204 + t63 * t720 * t212 + t817 * t192 + t106 * t232 * t166 + t106 * t67 * t2117 + t935 - t937 - 0.3e1 / 0.2e1 * t563 * t2090 * t204 - t520 * t2090 * t212 / 2 - t520 * t2156 * t204 / 2 - 0.3e1 / 0.2e1 * t579 * t2156 * t212 - 0.3e1 / 0.2e1 * t588 * t2093 * t204 - t512 * t2093 * t212 / 2 - t512 * t2169 * t204 / 2 - 0.3e1 / 0.2e1 * t604 * t2169 * t212 - 0.3e1 / 0.2e1 * t539 * t2096 * t204 - t516 * t2096 * t212 / 2 - t516 * t2182 * t204 / 2 - 0.3e1 / 0.2e1 * t555 * t2182 * t212;
	double K0304 = 0.3e1 / 0.4e1 * t6 * t17 * (t2140 + t2189);
	double t2202 = 2 * t19 * t161 - 2 * t20 + 2 * t23;
	double t2210 = 2 * t249 * t172 + 2 * t42 - 2 * t45;
	double t2231 = -2 * t169 * t257 - 2 * t246 * t179 + t881 * t192 + t106 * t271 * t166 + t106 * t67 * t2202 + t266 * t194 + t66 * t894 * t176 + t66 * t112 * t2210 + t906 * t167 + t75 * t257 * t166 + t75 * t61 * t2202 + t705 * t244 + t169 * t255 + t246 * t177 + t39 * t913 * t176 + t39 * t86 * t2210 + t75 * t179 * t243 + t39 * t710 * t254 + t94 * t64 * t2202;
	double t2276 = t713 * t260 + t184 * t262 + t63 * t100 * t2210 + t94 * t189 * t243 + t63 * t720 * t254 - t516 * t2096 * t254 / 2 - t516 * t2182 * t243 / 2 - 0.3e1 / 0.2e1 * t555 * t2182 * t254 - 0.3e1 / 0.2e1 * t563 * t2090 * t243 - t520 * t2090 * t254 / 2 - t520 * t2156 * t243 / 2 - 0.3e1 / 0.2e1 * t579 * t2156 * t254 - 0.3e1 / 0.2e1 * t588 * t2093 * t243 - t512 * t2093 * t254 / 2 - t512 * t2169 * t243 / 2 - 0.3e1 / 0.2e1 * t604 * t2169 * t254 - 0.3e1 / 0.2e1 * t539 * t2096 * t243 + t1748 - t1750;
	double K0305 = 0.3e1 / 0.4e1 * t6 * t17 * (t2231 + t2276);
	double t2294 = 2 * t278 * t161 + 2 * t28 * t163;
	double t2305 = 2 * t288 * t172 + 2 * t290 * t50;
	double t2319 = -2 * t169 * t296 - 2 * t285 * t179 - 2 * t184 * t306 - 2 * t301 * t189 + t1014 * t167 + t75 * t296 * t166 + t75 * t61 * t2294 + t705 * t283 + t169 * t294 + t285 * t177 + t39 * t974 * t176 + t39 * t86 * t2305 + t75 * t179 * t282 + t39 * t710 * t293 + t982 * t182 + t94 * t306 * t166 + t94 * t64 * t2294 + t713 * t299 + t184 * t304;
	double t2369 = t301 * t187 + t63 * t995 * t176 + t63 * t100 * t2305 + t94 * t189 * t282 + t63 * t720 * t293 + t106 * t67 * t2294 + t66 * t112 * t2305 - t520 * t2156 * t282 / 2 - 0.3e1 / 0.2e1 * t579 * t2156 * t293 - 0.3e1 / 0.2e1 * t588 * t2093 * t282 - t512 * t2093 * t293 / 2 - t512 * t2169 * t282 / 2 - 0.3e1 / 0.2e1 * t604 * t2169 * t293 - 0.3e1 / 0.2e1 * t539 * t2096 * t282 - t516 * t2096 * t293 / 2 - t516 * t2182 * t282 / 2 - 0.3e1 / 0.2e1 * t555 * t2182 * t293 - 0.3e1 / 0.2e1 * t563 * t2090 * t282 - t520 * t2090 * t293 / 2;
	double K0306 = 0.3e1 / 0.4e1 * t6 * t17 * (t2319 + t2369);
	double t2378 = 2 * t21 * t163 + 2 * t27 - 2 * t29;
	double t2412 = -0.3e1 / 0.2e1 * t563 * t2090 * t320 + t75 * t61 * t2378 + t705 * t321 - t520 * t2156 * t320 / 2 + t75 * t179 * t320 - 0.3e1 / 0.2e1 * t588 * t2093 * t320 + t1044 * t182 + t94 * t64 * t2378 + t713 * t323 + t937 - t512 * t2169 * t320 / 2 + t325 * t187 + t94 * t189 * t320 - 2 * t325 * t189 - 0.3e1 / 0.2e1 * t539 * t2096 * t320 + t1060 * t192 + t106 * t67 * t2378 - t516 * t2182 * t320 / 2 + t330 * t194;
	double K0307 = 0.3e1 / 0.4e1 * t6 * t17 * t2412;
	double t2421 = 2 * t43 * t161 + 2 * t20 - 2 * t23;
	double t2454 = -0.3e1 / 0.2e1 * t563 * t2090 * t340 + t1079 * t167 + t75 * t61 * t2421 + t705 * t341 - t1748 - t520 * t2156 * t340 / 2 + t343 * t177 + t75 * t179 * t340 - 2 * t343 * t179 - 0.3e1 / 0.2e1 * t588 * t2093 * t340 + t94 * t64 * t2421 + t713 * t346 - t512 * t2169 * t340 / 2 + t94 * t189 * t340 - 0.3e1 / 0.2e1 * t539 * t2096 * t340 + t1104 * t192 + t106 * t67 * t2421 - t516 * t2182 * t340 / 2 + t350 * t194;
	double K0308 = 0.3e1 / 0.4e1 * t6 * t17 * t2454;
	double t2464 = 2 * t18 * t161 + 2 * t48 * t163;
	double t2499 = -0.3e1 / 0.2e1 * t563 * t2090 * t360 + t1123 * t167 + t75 * t61 * t2464 + t705 * t361 - t520 * t2156 * t360 / 2 + t363 * t177 + t75 * t179 * t360 - 2 * t363 * t179 - 0.3e1 / 0.2e1 * t588 * t2093 * t360 + t1137 * t182 + t94 * t64 * t2464 + t713 * t366 - t512 * t2169 * t360 / 2 + t368 * t187 + t94 * t189 * t360 - 2 * t368 * t189 - 0.3e1 / 0.2e1 * t539 * t2096 * t360 + t106 * t67 * t2464 - t516 * t2182 * t360 / 2;
	double K0309 = 0.3e1 / 0.4e1 * t6 * t17 * t2499;
	double t2511 = 4 * t51 - 2 * t49;
	double t2545 = -t520 * t2090 * t380 / 2 + t169 * t381 - 0.3e1 / 0.2e1 * t579 * t2156 * t380 + t39 * t86 * t2511 + t39 * t710 * t380 - t512 * t2093 * t380 / 2 + t94 * t385 * t166 + t184 * t383 - 2 * t184 * t385 - 0.3e1 / 0.2e1 * t604 * t2169 * t380 + t63 * t1187 * t176 + t63 * t100 * t2511 + t63 * t720 * t380 - t935 - t516 * t2096 * t380 / 2 + t106 * t390 * t166 - 0.3e1 / 0.2e1 * t555 * t2182 * t380 + t66 * t1205 * t176 + t66 * t112 * t2511;
	double K0310 = 0.3e1 / 0.4e1 * t6 * t17 * t2545;
	double t2563 = 2 * t21 * t172 - 2 * t42 + 2 * t45;
	double t2591 = -t520 * t2090 * t400 / 2 + t75 * t403 * t166 + t169 * t401 - 2 * t169 * t403 - 0.3e1 / 0.2e1 * t579 * t2156 * t400 + t39 * t1223 * t176 + t39 * t86 * t2563 + t39 * t710 * t400 + t1750 - t512 * t2093 * t400 / 2 + t184 * t406 - 0.3e1 / 0.2e1 * t604 * t2169 * t400 + t63 * t100 * t2563 + t63 * t720 * t400 - t516 * t2096 * t400 / 2 + t106 * t410 * t166 - 0.3e1 / 0.2e1 * t555 * t2182 * t400 + t66 * t1253 * t176 + t66 * t112 * t2563;
	double K0311 = 0.3e1 / 0.4e1 * t6 * t17 * t2591;
	double t2610 = 2 * t40 * t172 + 2 * t26 * t50;
	double t2640 = -t520 * t2090 * t420 / 2 + t75 * t423 * t166 + t169 * t421 - 2 * t169 * t423 - 0.3e1 / 0.2e1 * t579 * t2156 * t420 + t39 * t1271 * t176 + t39 * t86 * t2610 + t39 * t710 * t420 - t512 * t2093 * t420 / 2 + t94 * t428 * t166 + t184 * t426 - 2 * t184 * t428 - 0.3e1 / 0.2e1 * t604 * t2169 * t420 + t63 * t1290 * t176 + t63 * t100 * t2610 + t63 * t720 * t420 - t516 * t2096 * t420 / 2 - 0.3e1 / 0.2e1 * t555 * t2182 * t420 + t66 * t112 * t2610;
	double K0312 = 0.3e1 / 0.4e1 * t6 * t17 * t2640;
	double K0401 = K0104;
	double K0402 = K0204;
	double K0403 = K0304;
	double t2647 = t204 * t204;
	double t2651 = t212 * t212;
	double t2667 = t200 * t200;
	double t2668 = t22 * t22;
	double t2670 = 2 * t2667 + 2 * t2668;
	double t2678 = t207 * t207;
	double t2679 = t209 * t209;
	double t2681 = 2 * t2678 + 2 * t2679;
	double t2684 = -4 * t217 * t222 - 4 * t227 * t232 - 0.3e1 / 0.2e1 * t465 * t61 * t2647 - 0.3e1 / 0.2e1 * t39 * t440 * t2651 - 0.3e1 / 0.2e1 * t448 * t64 * t2647 - 0.3e1 / 0.2e1 * t63 * t453 * t2651 - 0.3e1 / 0.2e1 * t457 * t67 * t2647 - 0.3e1 / 0.2e1 * t66 * t461 * t2651 + t94 * t64 * t2670 + 2 * t217 * t220 + 2 * t63 * t808 * t212 + t63 * t100 * t2681;
	double t2708 = t46 * t204;
	double t2711 = t52 * t204;
	double t2714 = t56 * t204;
	double t2717 = 2 * t106 * t232 * t204 + t106 * t67 * t2670 + t66 * t112 * t2681 + 2 * t94 * t222 * t204 - t520 * t2708 * t212 - t512 * t2711 * t212 - t516 * t2714 * t212 + 2 * t66 * t778 * t212 + t75 * t61 * t2670 + t39 * t86 * t2681 + 2 * t795 * t215 + 2 * t817 * t225 + 2 * t227 * t230;
	double K0404 = 0.3e1 / 0.4e1 * t6 * t17 * (t2684 + t2717);
	double t2749 = -2 * t227 * t271 - 2 * t266 * t232 + t906 * t205 + t75 * t257 * t204 + t246 * t213 + t39 * t913 * t212 + t795 * t260 + t217 * t262 + t94 * t222 * t243 + t63 * t808 * t254 + t881 * t225 + t106 * t271 * t204 + t817 * t264 + t227 * t269 + t266 * t230 + t66 * t894 * t212 + t106 * t232 * t243 + t66 * t778 * t254;
	double t2760 = t46 * t212;
	double t2781 = t52 * t212;
	double t2802 = t56 * t212;
	double t2812 = -0.3e1 / 0.2e1 * t563 * t2708 * t243 - t520 * t2708 * t254 / 2 + 2 * t570 * t46 * t240 * t22 - t520 * t2760 * t243 / 2 - 0.3e1 / 0.2e1 * t579 * t2760 * t254 + 2 * t583 * t46 * t251 * t209 - 0.3e1 / 0.2e1 * t588 * t2711 * t243 - t512 * t2711 * t254 / 2 + 2 * t595 * t52 * t240 * t22 - t512 * t2781 * t243 / 2 - 0.3e1 / 0.2e1 * t604 * t2781 * t254 + 2 * t534 * t52 * t251 * t209 - 0.3e1 / 0.2e1 * t539 * t2714 * t243 - t516 * t2714 * t254 / 2 + 2 * t546 * t56 * t240 * t22 - t516 * t2802 * t243 / 2 - 0.3e1 / 0.2e1 * t555 * t2802 * t254 + 2 * t559 * t252 * t209;
	double K0405 = 0.3e1 / 0.4e1 * t6 * t17 * (t2749 + t2812);
	double t2844 = -2 * t217 * t306 - 2 * t301 * t222 + t1014 * t205 + t75 * t296 * t204 + t285 * t213 + t39 * t974 * t212 + t982 * t215 + t94 * t306 * t204 + t795 * t299 + t217 * t304 + t301 * t220 + t63 * t995 * t212 + t94 * t222 * t282 + t63 * t808 * t293 + t817 * t309 + t227 * t311 + t106 * t232 * t282 + t66 * t778 * t293;
	double t2904 = -0.3e1 / 0.2e1 * t563 * t2708 * t282 - t520 * t2708 * t293 / 2 + 2 * t570 * t46 * t28 * t200 - t520 * t2760 * t282 / 2 - 0.3e1 / 0.2e1 * t579 * t2760 * t293 + 2 * t583 * t46 * t290 * t207 - 0.3e1 / 0.2e1 * t588 * t2711 * t282 - t512 * t2711 * t293 / 2 + 2 * t595 * t52 * t28 * t200 - t512 * t2781 * t282 / 2 - 0.3e1 / 0.2e1 * t604 * t2781 * t293 + 2 * t534 * t291 * t207 - 0.3e1 / 0.2e1 * t539 * t2714 * t282 - t516 * t2714 * t293 / 2 + 2 * t546 * t56 * t28 * t200 - t516 * t2802 * t282 / 2 - 0.3e1 / 0.2e1 * t555 * t2802 * t293 + 2 * t559 * t56 * t290 * t207;
	double K0406 = 0.3e1 / 0.4e1 * t6 * t17 * (t2844 + t2904);
	double t2914 = 2 * t21 * t200 + 2 * t40 * t22;
	double t2950 = -0.3e1 / 0.2e1 * t563 * t2708 * t320 + t75 * t61 * t2914 - t520 * t2760 * t320 / 2 - 0.3e1 / 0.2e1 * t588 * t2711 * t320 + t1044 * t215 + t94 * t64 * t2914 + t795 * t323 - t512 * t2781 * t320 / 2 + t325 * t220 + t94 * t222 * t320 - 2 * t325 * t222 - 0.3e1 / 0.2e1 * t539 * t2714 * t320 + t1060 * t225 + t106 * t67 * t2914 + t817 * t328 - t516 * t2802 * t320 / 2 + t330 * t230 + t106 * t232 * t320 - 2 * t330 * t232;
	double K0407 = 0.3e1 / 0.4e1 * t6 * t17 * t2950;
	double t2959 = 4 * t32 - 2 * t33;
	double t2992 = -0.3e1 / 0.2e1 * t563 * t2708 * t340 + t1079 * t205 + t75 * t61 * t2959 - t520 * t2760 * t340 / 2 + t343 * t213 - 0.3e1 / 0.2e1 * t588 * t2711 * t340 + t94 * t64 * t2959 + t795 * t346 - t512 * t2781 * t340 / 2 + t94 * t222 * t340 - 0.3e1 / 0.2e1 * t539 * t2714 * t340 + t1104 * t225 + t106 * t67 * t2959 + t817 * t348 - t842 - t516 * t2802 * t340 / 2 + t350 * t230 + t106 * t232 * t340 - 2 * t350 * t232;
	double K0408 = 0.3e1 / 0.4e1 * t6 * t17 * t2992;
	double t3001 = 2 * t48 * t200 + 2 * t27 - 2 * t29;
	double t3034 = -0.3e1 / 0.2e1 * t563 * t2708 * t360 + t1123 * t205 + t75 * t61 * t3001 - t520 * t2760 * t360 / 2 + t363 * t213 - 0.3e1 / 0.2e1 * t588 * t2711 * t360 + t1137 * t215 + t94 * t64 * t3001 + t795 * t366 + t937 - t512 * t2781 * t360 / 2 + t368 * t220 + t94 * t222 * t360 - 2 * t368 * t222 - 0.3e1 / 0.2e1 * t539 * t2714 * t360 + t106 * t67 * t3001 + t817 * t371 - t516 * t2802 * t360 / 2 + t106 * t232 * t360;
	double K0409 = 0.3e1 / 0.4e1 * t6 * t17 * t3034;
	double t3046 = 2 * t18 * t209 + 2 * t43 * t207;
	double t3083 = -t520 * t2708 * t380 / 2 - 0.3e1 / 0.2e1 * t579 * t2760 * t380 + t39 * t86 * t3046 - t512 * t2711 * t380 / 2 + t94 * t385 * t204 + t217 * t383 - 2 * t217 * t385 - 0.3e1 / 0.2e1 * t604 * t2781 * t380 + t63 * t1187 * t212 + t63 * t100 * t3046 + t63 * t808 * t380 - t516 * t2714 * t380 / 2 + t106 * t390 * t204 + t227 * t388 - 2 * t227 * t390 - 0.3e1 / 0.2e1 * t555 * t2802 * t380 + t66 * t1205 * t212 + t66 * t112 * t3046 + t66 * t778 * t380;
	double K0410 = 0.3e1 / 0.4e1 * t6 * t17 * t3083;
	double t3098 = 2 * t48 * t209 - 2 * t54 + 2 * t55;
	double t3129 = -t520 * t2708 * t400 / 2 + t75 * t403 * t204 - 0.3e1 / 0.2e1 * t579 * t2760 * t400 + t39 * t1223 * t212 + t39 * t86 * t3098 - t512 * t2711 * t400 / 2 + t217 * t406 - 0.3e1 / 0.2e1 * t604 * t2781 * t400 + t63 * t100 * t3098 + t63 * t808 * t400 - t516 * t2714 * t400 / 2 + t106 * t410 * t204 + t227 * t408 - 2 * t227 * t410 - 0.3e1 / 0.2e1 * t555 * t2802 * t400 + t66 * t1253 * t212 + t66 * t112 * t3098 + t66 * t778 * t400 + t839;
	double K0411 = 0.3e1 / 0.4e1 * t6 * t17 * t3129;
	double t3144 = 2 * t26 * t207 - 2 * t49 + 2 * t51;
	double t3175 = -t520 * t2708 * t420 / 2 + t75 * t423 * t204 - 0.3e1 / 0.2e1 * t579 * t2760 * t420 + t39 * t1271 * t212 + t39 * t86 * t3144 - t512 * t2711 * t420 / 2 + t94 * t428 * t204 + t217 * t426 - 2 * t217 * t428 - 0.3e1 / 0.2e1 * t604 * t2781 * t420 + t63 * t1290 * t212 + t63 * t100 * t3144 + t63 * t808 * t420 - t935 - t516 * t2714 * t420 / 2 + t227 * t431 - 0.3e1 / 0.2e1 * t555 * t2802 * t420 + t66 * t112 * t3144 + t66 * t778 * t420;
	double K0412 = 0.3e1 / 0.4e1 * t6 * t17 * t3175;
	double K0501 = K0105;
	double K0502 = K0205;
	double K0503 = K0305;
	double K0504 = K0405;
	double t3187 = t19 * t19;
	double t3188 = t240 * t240;
	double t3190 = 2 * t3187 + 2 * t3188;
	double t3198 = t249 * t249;
	double t3199 = t251 * t251;
	double t3201 = 2 * t3198 + 2 * t3199;
	double t3213 = t63 * t100 * t3201 + 2 * t106 * t271 * t243 + 2 * t75 * t257 * t243 + 2 * t39 * t913 * t254 + t75 * t61 * t3190 + t94 * t64 * t3190 + t39 * t86 * t3201 + 2 * t906 * t244 + 2 * t246 * t255 - 4 * t246 * t257 + 2 * t881 * t264 - 4 * t266 * t271;
	double t3223 = t243 * t243;
	double t3227 = t254 * t254;
	double t3243 = t46 * t243;
	double t3246 = t52 * t243;
	double t3249 = t56 * t243;
	double t3252 = t106 * t67 * t3190 + 2 * t266 * t269 + 2 * t66 * t894 * t254 + t66 * t112 * t3201 - 0.3e1 / 0.2e1 * t465 * t61 * t3223 - 0.3e1 / 0.2e1 * t39 * t440 * t3227 - 0.3e1 / 0.2e1 * t448 * t64 * t3223 - 0.3e1 / 0.2e1 * t63 * t453 * t3227 - 0.3e1 / 0.2e1 * t457 * t67 * t3223 - 0.3e1 / 0.2e1 * t66 * t461 * t3227 - t520 * t3243 * t254 - t512 * t3246 * t254 - t516 * t3249 * t254;
	double K0505 = 0.3e1 / 0.4e1 * t6 * t17 * (t3213 + t3252);
	double t3284 = -2 * t246 * t296 - 2 * t285 * t257 + t1014 * t244 + t75 * t296 * t243 + t906 * t283 + t246 * t294 + t285 * t255 + t39 * t974 * t254 + t75 * t257 * t282 + t39 * t913 * t293 + t982 * t260 + t94 * t306 * t243 + t301 * t262 + t63 * t995 * t254 + t881 * t309 + t266 * t311 + t106 * t271 * t282 + t66 * t894 * t293;
	double t3285 = t46 * t254;
	double t3302 = t52 * t254;
	double t3323 = t56 * t254;
	double t3347 = -0.3e1 / 0.2e1 * t579 * t3285 * t293 + 2 * t583 * t289 * t249 - 0.3e1 / 0.2e1 * t588 * t3246 * t282 - t512 * t3246 * t293 / 2 + 2 * t595 * t52 * t278 * t19 - t512 * t3302 * t282 / 2 - 0.3e1 / 0.2e1 * t604 * t3302 * t293 + 2 * t534 * t52 * t288 * t249 - 0.3e1 / 0.2e1 * t539 * t3249 * t282 - t516 * t3249 * t293 / 2 + 2 * t546 * t56 * t278 * t19 - t516 * t3323 * t282 / 2 - 0.3e1 / 0.2e1 * t555 * t3323 * t293 + 2 * t559 * t56 * t288 * t249 - 0.3e1 / 0.2e1 * t563 * t3243 * t282 - t520 * t3243 * t293 / 2 + 2 * t570 * t46 * t278 * t19 - t520 * t3285 * t282 / 2;
	double K0506 = 0.3e1 / 0.4e1 * t6 * t17 * (t3284 + t3347);
	double t3356 = 2 * t40 * t240 - 2 * t32 + 2 * t33;
	double t3390 = -0.3e1 / 0.2e1 * t563 * t3243 * t320 + t75 * t61 * t3356 + t906 * t321 - t520 * t3285 * t320 / 2 + t75 * t257 * t320 - 0.3e1 / 0.2e1 * t588 * t3246 * t320 + t1044 * t260 + t94 * t64 * t3356 - t512 * t3302 * t320 / 2 + t325 * t262 - 0.3e1 / 0.2e1 * t539 * t3249 * t320 + t1060 * t264 + t106 * t67 * t3356 + t881 * t328 + t842 - t516 * t3323 * t320 / 2 + t330 * t269 + t106 * t271 * t320 - 2 * t330 * t271;
	double K0507 = 0.3e1 / 0.4e1 * t6 * t17 * t3390;
	double t3400 = 2 * t43 * t19 + 2 * t26 * t240;
	double t3435 = -0.3e1 / 0.2e1 * t563 * t3243 * t340 + t1079 * t244 + t75 * t61 * t3400 + t906 * t341 - t520 * t3285 * t340 / 2 + t343 * t255 + t75 * t257 * t340 - 2 * t343 * t257 - 0.3e1 / 0.2e1 * t588 * t3246 * t340 + t94 * t64 * t3400 - t512 * t3302 * t340 / 2 - 0.3e1 / 0.2e1 * t539 * t3249 * t340 + t1104 * t264 + t106 * t67 * t3400 + t881 * t348 - t516 * t3323 * t340 / 2 + t350 * t269 + t106 * t271 * t340 - 2 * t350 * t271;
	double K0508 = 0.3e1 / 0.4e1 * t6 * t17 * t3435;
	double t3444 = 4 * t20 - 2 * t23;
	double t3477 = -0.3e1 / 0.2e1 * t563 * t3243 * t360 + t1123 * t244 + t75 * t61 * t3444 + t906 * t361 - t1748 - t520 * t3285 * t360 / 2 + t363 * t255 + t75 * t257 * t360 - 2 * t363 * t257 - 0.3e1 / 0.2e1 * t588 * t3246 * t360 + t1137 * t260 + t94 * t64 * t3444 - t512 * t3302 * t360 / 2 + t368 * t262 - 0.3e1 / 0.2e1 * t539 * t3249 * t360 + t106 * t67 * t3444 + t881 * t371 - t516 * t3323 * t360 / 2 + t106 * t271 * t360;
	double K0509 = 0.3e1 / 0.4e1 * t6 * t17 * t3477;
	double t3489 = 2 * t18 * t251 + 2 * t54 - 2 * t55;
	double t3523 = -t520 * t3243 * t380 / 2 + t246 * t381 - 0.3e1 / 0.2e1 * t579 * t3285 * t380 + t39 * t86 * t3489 + t39 * t913 * t380 - t512 * t3246 * t380 / 2 + t94 * t385 * t243 - 0.3e1 / 0.2e1 * t604 * t3302 * t380 + t63 * t1187 * t254 + t63 * t100 * t3489 - t516 * t3249 * t380 / 2 + t106 * t390 * t243 + t266 * t388 - 2 * t266 * t390 - 0.3e1 / 0.2e1 * t555 * t3323 * t380 + t66 * t1205 * t254 + t66 * t112 * t3489 + t66 * t894 * t380 - t839;
	double K0510 = 0.3e1 / 0.4e1 * t6 * t17 * t3523;
	double t3542 = 2 * t21 * t249 + 2 * t48 * t251;
	double t3572 = -t520 * t3243 * t400 / 2 + t75 * t403 * t243 + t246 * t401 - 2 * t246 * t403 - 0.3e1 / 0.2e1 * t579 * t3285 * t400 + t39 * t1223 * t254 + t39 * t86 * t3542 + t39 * t913 * t400 - t512 * t3246 * t400 / 2 - 0.3e1 / 0.2e1 * t604 * t3302 * t400 + t63 * t100 * t3542 - t516 * t3249 * t400 / 2 + t106 * t410 * t243 + t266 * t408 - 2 * t266 * t410 - 0.3e1 / 0.2e1 * t555 * t3323 * t400 + t66 * t1253 * t254 + t66 * t112 * t3542 + t66 * t894 * t400;
	double K0511 = 0.3e1 / 0.4e1 * t6 * t17 * t3572;
	double t3590 = 2 * t40 * t249 - 2 * t42 + 2 * t45;
	double t3618 = -t520 * t3243 * t420 / 2 + t75 * t423 * t243 + t246 * t421 - 2 * t246 * t423 - 0.3e1 / 0.2e1 * t579 * t3285 * t420 + t39 * t1271 * t254 + t39 * t86 * t3590 + t39 * t913 * t420 + t1750 - t512 * t3246 * t420 / 2 + t94 * t428 * t243 - 0.3e1 / 0.2e1 * t604 * t3302 * t420 + t63 * t1290 * t254 + t63 * t100 * t3590 - t516 * t3249 * t420 / 2 + t266 * t431 - 0.3e1 / 0.2e1 * t555 * t3323 * t420 + t66 * t112 * t3590 + t66 * t894 * t420;
	double K0512 = 0.3e1 / 0.4e1 * t6 * t17 * t3618;
	double K0601 = K0106;
	double K0602 = K0206;
	double K0603 = K0306;
	double K0604 = K0406;
	double K0605 = K0506;
	double t3625 = t46 * t282;
	double t3628 = t52 * t282;
	double t3631 = t56 * t282;
	double t3639 = t278 * t278;
	double t3640 = t28 * t28;
	double t3642 = 2 * t3639 + 2 * t3640;
	double t3650 = t288 * t288;
	double t3651 = t290 * t290;
	double t3653 = 2 * t3650 + 2 * t3651;
	double t3658 = 2 * t75 * t296 * t282 - t520 * t3625 * t293 - t512 * t3628 * t293 - t516 * t3631 * t293 + 2 * t39 * t974 * t293 + t75 * t61 * t3642 + t39 * t86 * t3653 + 2 * t1014 * t283 + 2 * t285 * t294 - 4 * t285 * t296 + 2 * t982 * t299 - 4 * t301 * t306;
	double t3675 = t282 * t282;
	double t3679 = t293 * t293;
	double t3695 = 2 * t94 * t306 * t282 + t94 * t64 * t3642 + 2 * t301 * t304 + 2 * t63 * t995 * t293 + t63 * t100 * t3653 + t106 * t67 * t3642 + t66 * t112 * t3653 - 0.3e1 / 0.2e1 * t465 * t61 * t3675 - 0.3e1 / 0.2e1 * t39 * t440 * t3679 - 0.3e1 / 0.2e1 * t448 * t64 * t3675 - 0.3e1 / 0.2e1 * t63 * t453 * t3679 - 0.3e1 / 0.2e1 * t457 * t67 * t3675 - 0.3e1 / 0.2e1 * t66 * t461 * t3679;
	double K0606 = 0.3e1 / 0.4e1 * t6 * t17 * (t3658 + t3695);
	double t3704 = 4 * t29 - 2 * t27;
	double t3708 = t46 * t293;
	double t3721 = t52 * t293;
	double t3736 = t56 * t293;
	double t3741 = -0.3e1 / 0.2e1 * t563 * t3625 * t320 + t75 * t61 * t3704 + t1014 * t321 - t520 * t3708 * t320 / 2 + t75 * t296 * t320 - 0.3e1 / 0.2e1 * t588 * t3628 * t320 + t1044 * t299 + t94 * t64 * t3704 + t982 * t323 - t937 - t512 * t3721 * t320 / 2 + t325 * t304 + t94 * t306 * t320 - 2 * t325 * t306 - 0.3e1 / 0.2e1 * t539 * t3631 * t320 + t1060 * t309 + t106 * t67 * t3704 - t516 * t3736 * t320 / 2 + t330 * t311;
	double K0607 = 0.3e1 / 0.4e1 * t6 * t17 * t3741;
	double t3750 = 2 * t43 * t278 - 2 * t20 + 2 * t23;
	double t3783 = -0.3e1 / 0.2e1 * t563 * t3625 * t340 + t1079 * t283 + t75 * t61 * t3750 + t1014 * t341 + t1748 - t520 * t3708 * t340 / 2 + t343 * t294 + t75 * t296 * t340 - 2 * t343 * t296 - 0.3e1 / 0.2e1 * t588 * t3628 * t340 + t94 * t64 * t3750 + t982 * t346 - t512 * t3721 * t340 / 2 + t94 * t306 * t340 - 0.3e1 / 0.2e1 * t539 * t3631 * t340 + t1104 * t309 + t106 * t67 * t3750 - t516 * t3736 * t340 / 2 + t350 * t311;
	double K0608 = 0.3e1 / 0.4e1 * t6 * t17 * t3783;
	double t3793 = 2 * t18 * t278 + 2 * t48 * t28;
	double t3828 = -0.3e1 / 0.2e1 * t563 * t3625 * t360 + t1123 * t283 + t75 * t61 * t3793 + t1014 * t361 - t520 * t3708 * t360 / 2 + t363 * t294 + t75 * t296 * t360 - 2 * t363 * t296 - 0.3e1 / 0.2e1 * t588 * t3628 * t360 + t1137 * t299 + t94 * t64 * t3793 + t982 * t366 - t512 * t3721 * t360 / 2 + t368 * t304 + t94 * t306 * t360 - 2 * t368 * t306 - 0.3e1 / 0.2e1 * t539 * t3631 * t360 + t106 * t67 * t3793 - t516 * t3736 * t360 / 2;
	double K0609 = 0.3e1 / 0.4e1 * t6 * t17 * t3828;
	double t3840 = 2 * t43 * t290 + 2 * t49 - 2 * t51;
	double t3874 = -t520 * t3625 * t380 / 2 + t285 * t381 - 0.3e1 / 0.2e1 * t579 * t3708 * t380 + t39 * t86 * t3840 + t39 * t974 * t380 - t512 * t3628 * t380 / 2 + t94 * t385 * t282 + t301 * t383 - 2 * t301 * t385 - 0.3e1 / 0.2e1 * t604 * t3721 * t380 + t63 * t1187 * t293 + t63 * t100 * t3840 + t63 * t995 * t380 + t935 - t516 * t3631 * t380 / 2 + t106 * t390 * t282 - 0.3e1 / 0.2e1 * t555 * t3736 * t380 + t66 * t1205 * t293 + t66 * t112 * t3840;
	double K0610 = 0.3e1 / 0.4e1 * t6 * t17 * t3874;
	double t3892 = 2 * t21 * t288 + 2 * t42 - 2 * t45;
	double t3920 = -t520 * t3625 * t400 / 2 + t75 * t403 * t282 + t285 * t401 - 2 * t285 * t403 - 0.3e1 / 0.2e1 * t579 * t3708 * t400 + t39 * t1223 * t293 + t39 * t86 * t3892 + t39 * t974 * t400 - t1750 - t512 * t3628 * t400 / 2 + t301 * t406 - 0.3e1 / 0.2e1 * t604 * t3721 * t400 + t63 * t100 * t3892 + t63 * t995 * t400 - t516 * t3631 * t400 / 2 + t106 * t410 * t282 - 0.3e1 / 0.2e1 * t555 * t3736 * t400 + t66 * t1253 * t293 + t66 * t112 * t3892;
	double K0611 = 0.3e1 / 0.4e1 * t6 * t17 * t3920;
	double t3939 = 2 * t26 * t290 + 2 * t40 * t288;
	double t3969 = -t520 * t3625 * t420 / 2 + t75 * t423 * t282 + t285 * t421 - 2 * t285 * t423 - 0.3e1 / 0.2e1 * t579 * t3708 * t420 + t39 * t1271 * t293 + t39 * t86 * t3939 + t39 * t974 * t420 - t512 * t3628 * t420 / 2 + t94 * t428 * t282 + t301 * t426 - 2 * t301 * t428 - 0.3e1 / 0.2e1 * t604 * t3721 * t420 + t63 * t1290 * t293 + t63 * t100 * t3939 + t63 * t995 * t420 - t516 * t3631 * t420 / 2 - 0.3e1 / 0.2e1 * t555 * t3736 * t420 + t66 * t112 * t3939;
	double K0612 = 0.3e1 / 0.4e1 * t6 * t17 * t3969;
	double K0701 = K0107;
	double K0702 = K0207;
	double K0703 = K0307;
	double K0704 = K0407;
	double K0705 = K0507;
	double K0706 = K0607;
	double t3972 = t320 * t320;
	double t3976 = t21 * t21;
	double t3977 = t40 * t40;
	double t3979 = 2 * t3976 + 2 * t3977;
	double K0707 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t465 * t61 * t3972 + t75 * t61 * t3979 - 0.3e1 / 0.2e1 * t448 * t64 * t3972 + 2 * t1044 * t323 + t94 * t64 * t3979 - 0.3e1 / 0.2e1 * t457 * t67 * t3972 + 2 * t1060 * t328 + t106 * t67 * t3979);
	double t3999 = t46 * t320;
	double t4004 = t46 * t26;
	double t4008 = t52 * t320;
	double t4016 = t56 * t320;
	double t4021 = t56 * t26;
	double K0708 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t563 * t3999 * t340 + t1079 * t321 + 2 * t570 * t4004 * t40 - 0.3e1 / 0.2e1 * t588 * t4008 * t340 + 2 * t595 * t418 * t40 + t1044 * t346 - 0.3e1 / 0.2e1 * t539 * t4016 * t340 + t1104 * t328 + 2 * t546 * t4021 * t40 + t1060 * t348);
	double t4033 = t46 * t48;
	double t4041 = t52 * t48;
	double K0709 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t563 * t3999 * t360 + t1123 * t321 + 2 * t570 * t4033 * t21 - 0.3e1 / 0.2e1 * t588 * t4008 * t360 + t1137 * t323 + 2 * t595 * t4041 * t21 + t1044 * t366 - 0.3e1 / 0.2e1 * t539 * t4016 * t360 + 2 * t546 * t398 * t21 + t1060 * t371);
	double t4066 = 2 * t325 * t385;
	double t4074 = 2 * t330 * t390;
	double K0710 = 0.3e1 / 0.4e1 * t6 * t17 * (-t520 * t3999 * t380 / 2 - t512 * t4008 * t380 / 2 + t94 * t385 * t320 + t325 * t383 - t4066 - t516 * t4016 * t380 / 2 + t106 * t390 * t320 + t330 * t388 - t4074);
	double K0711 = 0.3e1 / 0.4e1 * t6 * t17 * (-t520 * t3999 * t400 / 2 + t75 * t403 * t320 - t512 * t4008 * t400 / 2 + t325 * t406 - t516 * t4016 * t400 / 2 + t106 * t410 * t320 + t330 * t408 - 2 * t330 * t410);
	double K0712 = 0.3e1 / 0.4e1 * t6 * t17 * (-t520 * t3999 * t420 / 2 + t75 * t423 * t320 - t512 * t4008 * t420 / 2 + t94 * t428 * t320 + t325 * t426 - 2 * t325 * t428 - t516 * t4016 * t420 / 2 + t330 * t431);
	double K0801 = K0108;
	double K0802 = K0208;
	double K0803 = K0308;
	double K0804 = K0408;
	double K0805 = K0508;
	double K0806 = K0608;
	double K0807 = K0708;
	double t4118 = t340 * t340;
	double t4124 = t43 * t43;
	double t4125 = t26 * t26;
	double t4127 = 2 * t4124 + 2 * t4125;
	double K0808 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t465 * t61 * t4118 + 2 * t1079 * t341 + t75 * t61 * t4127 - 0.3e1 / 0.2e1 * t448 * t64 * t4118 + t94 * t64 * t4127 - 0.3e1 / 0.2e1 * t457 * t67 * t4118 + 2 * t1104 * t348 + t106 * t67 * t4127);
	double t4145 = t46 * t340;
	double t4155 = t52 * t340;
	double t4164 = t56 * t340;
	double K0809 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t563 * t4145 * t360 + t1123 * t341 + 2 * t570 * t46 * t18 * t43 + t1079 * t361 - 0.3e1 / 0.2e1 * t588 * t4155 * t360 + t1137 * t346 + 2 * t595 * t52 * t18 * t43 - 0.3e1 / 0.2e1 * t539 * t4164 * t360 + 2 * t546 * t378 * t43 + t1104 * t371);
	double K0810 = 0.3e1 / 0.4e1 * t6 * t17 * (-t520 * t4145 * t380 / 2 + t343 * t381 - t512 * t4155 * t380 / 2 + t94 * t385 * t340 - t516 * t4164 * t380 / 2 + t106 * t390 * t340 + t350 * t388 - 2 * t350 * t390);
	double t4211 = 2 * t350 * t410;
	double K0811 = 0.3e1 / 0.4e1 * t6 * t17 * (-t520 * t4145 * t400 / 2 + t75 * t403 * t340 + t343 * t401 - t4066 - t512 * t4155 * t400 / 2 - t516 * t4164 * t400 / 2 + t106 * t410 * t340 + t350 * t408 - t4211);
	double K0812 = 0.3e1 / 0.4e1 * t6 * t17 * (-t520 * t4145 * t420 / 2 + t75 * t423 * t340 + t343 * t421 - 2 * t343 * t423 - t512 * t4155 * t420 / 2 + t94 * t428 * t340 - t516 * t4164 * t420 / 2 + t350 * t431);
	double K0901 = K0109;
	double K0902 = K0209;
	double K0903 = K0309;
	double K0904 = K0409;
	double K0905 = K0509;
	double K0906 = K0609;
	double K0907 = K0709;
	double K0908 = K0809;
	double t4235 = t360 * t360;
	double t4241 = t18 * t18;
	double t4242 = t48 * t48;
	double t4244 = 2 * t4241 + 2 * t4242;
	double K0909 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t465 * t61 * t4235 + 2 * t1123 * t361 + t75 * t61 * t4244 - 0.3e1 / 0.2e1 * t448 * t64 * t4235 + 2 * t1137 * t366 + t94 * t64 * t4244 - 0.3e1 / 0.2e1 * t457 * t67 * t4235 + t106 * t67 * t4244);
	double t4262 = t46 * t360;
	double t4267 = t52 * t360;
	double t4276 = t56 * t360;
	double K0910 = 0.3e1 / 0.4e1 * t6 * t17 * (-t520 * t4262 * t380 / 2 + t363 * t381 - t512 * t4267 * t380 / 2 + t94 * t385 * t360 + t368 * t383 - 2 * t368 * t385 - t516 * t4276 * t380 / 2 + t106 * t390 * t360);
	double K0911 = 0.3e1 / 0.4e1 * t6 * t17 * (-t520 * t4262 * t400 / 2 + t75 * t403 * t360 + t363 * t401 - 2 * t363 * t403 - t512 * t4267 * t400 / 2 + t368 * t406 - t516 * t4276 * t400 / 2 + t106 * t410 * t360);
	double K0912 = 0.3e1 / 0.4e1 * t6 * t17 * (-t520 * t4262 * t420 / 2 + t75 * t423 * t360 + t363 * t421 - t4074 - t512 * t4267 * t420 / 2 + t94 * t428 * t360 + t368 * t426 - t4211 - t516 * t4276 * t420 / 2);
	double K1001 = K0110;
	double K1002 = K0210;
	double K1003 = K0310;
	double K1004 = K0410;
	double K1005 = K0510;
	double K1006 = K0610;
	double K1007 = K0710;
	double K1008 = K0810;
	double K1009 = K0910;
	double t4323 = t380 * t380;
	double t4328 = 2 * t4124 + 2 * t4241;
	double K1010 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t39 * t440 * t4323 + t39 * t86 * t4328 - 0.3e1 / 0.2e1 * t63 * t453 * t4323 + 2 * t63 * t1187 * t380 + t63 * t100 * t4328 - 0.3e1 / 0.2e1 * t66 * t461 * t4323 + 2 * t66 * t1205 * t380 + t66 * t112 * t4328);
	double t4350 = t46 * t380;
	double t4359 = t52 * t380;
	double t4368 = t56 * t380;
	double K1011 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t579 * t4350 * t400 + t39 * t1223 * t380 + 2 * t583 * t4033 * t18 - 0.3e1 / 0.2e1 * t604 * t4359 * t400 + 2 * t534 * t4041 * t18 + t63 * t1187 * t400 - 0.3e1 / 0.2e1 * t555 * t4368 * t400 + t66 * t1253 * t380 + 2 * t559 * t398 * t18 + t66 * t1205 * t400);
	double K1012 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t579 * t4350 * t420 + t39 * t1271 * t380 + 2 * t583 * t4004 * t43 - 0.3e1 / 0.2e1 * t604 * t4359 * t420 + t63 * t1290 * t380 + 2 * t534 * t418 * t43 + t63 * t1187 * t420 - 0.3e1 / 0.2e1 * t555 * t4368 * t420 + 2 * t559 * t4021 * t43 + t66 * t1205 * t420);
	double K1101 = K0111;
	double K1102 = K0211;
	double K1103 = K0311;
	double K1104 = K0411;
	double K1105 = K0511;
	double K1106 = K0611;
	double K1107 = K0711;
	double K1108 = K0811;
	double K1109 = K0911;
	double K1110 = K1011;
	double t4411 = t400 * t400;
	double t4419 = 2 * t3976 + 2 * t4242;
	double K1111 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t39 * t440 * t4411 + 2 * t39 * t1223 * t400 + t39 * t86 * t4419 - 0.3e1 / 0.2e1 * t63 * t453 * t4411 + t63 * t100 * t4419 - 0.3e1 / 0.2e1 * t66 * t461 * t4411 + 2 * t66 * t1253 * t400 + t66 * t112 * t4419);
	double K1112 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t579 * t46 * t400 * t420 + t39 * t1271 * t400 + 2 * t583 * t417 * t21 + t39 * t1223 * t420 - 0.3e1 / 0.2e1 * t604 * t52 * t400 * t420 + t63 * t1290 * t400 + 2 * t534 * t52 * t40 * t21 - 0.3e1 / 0.2e1 * t555 * t56 * t400 * t420 + 2 * t559 * t56 * t40 * t21 + t66 * t1253 * t420);
	double K1201 = K0112;
	double K1202 = K0212;
	double K1203 = K0312;
	double K1204 = K0412;
	double K1205 = K0512;
	double K1206 = K0612;
	double K1207 = K0712;
	double K1208 = K0812;
	double K1209 = K0912;
	double K1210 = K1012;
	double K1211 = K1112;
	double t4472 = t420 * t420;
	double t4480 = 2 * t3977 + 2 * t4125;
	double K1212 = 0.3e1 / 0.4e1 * t6 * t17 * (-0.3e1 / 0.2e1 * t39 * t440 * t4472 + 2 * t39 * t1271 * t420 + t39 * t86 * t4480 - 0.3e1 / 0.2e1 * t63 * t453 * t4472 + 2 * t63 * t1290 * t420 + t63 * t100 * t4480 - 0.3e1 / 0.2e1 * t66 * t461 * t4472 + t66 * t112 * t4480);

	W = W00;
	f(0) = f01; f(1) = f02; f(2) = f03; f(3) = f04; f(4) = f05; f(5) = f06; f(6) = f07; f(7) = f08;
	f(8) = f09; f(9) = f10; f(10) = f11; f(11) = f12;

	K << K0101, K0102, K0103, K0104, K0105, K0106, K0107, K0108, K0109, K0110, K0111, K0112,
		K0201, K0202, K0203, K0204, K0205, K0206, K0207, K0208, K0209, K0210, K0211, K0212,
		K0301, K0302, K0303, K0304, K0305, K0306, K0307, K0308, K0309, K0310, K0311, K0312,
		K0401, K0402, K0403, K0404, K0405, K0406, K0407, K0408, K0409, K0410, K0411, K0412,
		K0501, K0502, K0503, K0504, K0505, K0506, K0507, K0508, K0509, K0510, K0511, K0512,
		K0601, K0602, K0603, K0604, K0605, K0606, K0607, K0608, K0609, K0610, K0611, K0612,
		K0701, K0702, K0703, K0704, K0705, K0706, K0707, K0708, K0709, K0710, K0711, K0712,
		K0801, K0802, K0803, K0804, K0805, K0806, K0807, K0808, K0809, K0810, K0811, K0812,
		K0901, K0902, K0903, K0904, K0905, K0906, K0907, K0908, K0909, K0910, K0911, K0912,
		K1001, K1002, K1003, K1004, K1005, K1006, K1007, K1008, K1009, K1010, K1011, K1012,
		K1101, K1102, K1103, K1104, K1105, K1106, K1107, K1108, K1109, K1110, K1111, K1112,
		K1201, K1202, K1203, K1204, K1205, K1206, K1207, K1208, K1209, K1210, K1211, K1212;
}

void Force::computeInertial(const Vec3& xa, const Vec3& xb, const Vec3& xc,
	const Vec3& Xa, const Vec3& Xb, const Vec3& Xc, Eigen::Vector3d gravity,
	double rho, double &W, Eigen::VectorXd &f, Eigen::MatrixXd &M)
{
	double xax = xa[0];
	double xay = xa[1];
	double xaz = xa[2];
	double xbx = xb[0];
	double xby = xb[1];
	double xbz = xb[2];
	double xcx = xc[0];
	double xcy = xc[1];
	double xcz = xc[2];
	double Xax = Xa[0];
	double Xay = Xa[1];
	double Xbx = Xb[0];
	double Xby = Xb[1];
	double Xcx = Xc[0];
	double Xcy = Xc[1];
	double gx = gravity(0);
	double gy = gravity(1);
	double gz = gravity(2);

	double t8 = rho * (Xax * Xby - Xax * Xcy - Xbx * Xay + Xcx * Xay + Xbx * Xcy - Xcx * Xby);
	double W00 = -t8 * (gx * (xbx - xcx) / 6 + gy * (xby - xcy) / 6 + gz * (xbz - xcz) / 6 + gx * (xax - xcx) / 6 + gy * (xay - xcy) / 6 + gz * (xaz - xcz) / 6 + gx * xcx / 2 + gy * xcy / 2 + gz * xcz / 2);
	double f01 = t8 * gx / 6;
	double f02 = t8 * gy / 6;
	double f03 = t8 * gz / 6;
	double f04 = f01;
	double f05 = f02;
	double f06 = f03;
	double f07 = f04;
	double f08 = f05;
	double f09 = f06;
	double M0101 = t8 / 12;
	double M0102 = 0;
	double M0103 = 0;
	double M0104 = t8 / 24;
	double M0105 = 0;
	double M0106 = 0;
	double M0107 = M0104;
	double M0108 = 0;
	double M0109 = 0;
	double M0201 = 0;
	double M0202 = M0101;
	double M0203 = 0;
	double M0204 = 0;
	double M0205 = M0107;
	double M0206 = 0;
	double M0207 = 0;
	double M0208 = M0205;
	double M0209 = 0;
	double M0301 = 0;
	double M0302 = 0;
	double M0303 = M0202;
	double M0304 = 0;
	double M0305 = 0;
	double M0306 = M0208;
	double M0307 = 0;
	double M0308 = 0;
	double M0309 = M0306;
	double M0401 = M0309;
	double M0402 = 0;
	double M0403 = 0;
	double M0404 = M0303;
	double M0405 = 0;
	double M0406 = 0;
	double M0407 = M0401;
	double M0408 = 0;
	double M0409 = 0;
	double M0501 = 0;
	double M0502 = M0407;
	double M0503 = 0;
	double M0504 = 0;
	double M0505 = M0404;
	double M0506 = 0;
	double M0507 = 0;
	double M0508 = M0502;
	double M0509 = 0;
	double M0601 = 0;
	double M0602 = 0;
	double M0603 = M0508;
	double M0604 = 0;
	double M0605 = 0;
	double M0606 = M0505;
	double M0607 = 0;
	double M0608 = 0;
	double M0609 = M0603;
	double M0701 = M0609;
	double M0702 = 0;
	double M0703 = 0;
	double M0704 = M0701;
	double M0705 = 0;
	double M0706 = 0;
	double M0707 = M0606;
	double M0708 = 0;
	double M0709 = 0;
	double M0801 = 0;
	double M0802 = M0704;
	double M0803 = 0;
	double M0804 = 0;
	double M0805 = M0802;
	double M0806 = 0;
	double M0807 = 0;
	double M0808 = M0707;
	double M0809 = 0;
	double M0901 = 0;
	double M0902 = 0;
	double M0903 = M0805;
	double M0904 = 0;
	double M0905 = 0;
	double M0906 = M0903;
	double M0907 = 0;
	double M0908 = 0;
	double M0909 = M0808;

	W = W00;
	f(0) = f01; f(1) = f02; f(2) = f03; f(3) = f04; f(4) = f05; f(5) = f06; f(6) = f07; f(7) = f08; f(8) = f09;
	M << M0101, M0102, M0103, M0104, M0105, M0106, M0107, M0108, M0109,
		M0201, M0202, M0203, M0204, M0205, M0206, M0207, M0208, M0209,
		M0301, M0302, M0303, M0304, M0305, M0306, M0307, M0308, M0309,
		M0401, M0402, M0403, M0404, M0405, M0406, M0407, M0408, M0409,
		M0501, M0502, M0503, M0504, M0505, M0506, M0507, M0508, M0509,
		M0601, M0602, M0603, M0604, M0605, M0606, M0607, M0608, M0609,
		M0701, M0702, M0703, M0704, M0705, M0706, M0707, M0708, M0709,
		M0801, M0802, M0803, M0804, M0805, M0806, M0807, M0808, M0809,
		M0901, M0902, M0903, M0904, M0905, M0906, M0907, M0908, M0909;
}


void Force::computeMembrane(const Vec3& xa, const Vec3& xb, const Vec3& xc,
	const Vec3& Xa, const Vec3& Xb, const Vec3& Xc,
	double e, double nu, Eigen::Matrix<double, 2, 3>& P, Eigen::Matrix2d& Q,
	double &W, Eigen::VectorXd& f, Eigen::MatrixXd& K)
{
	double xax = xa[0];
	double xay = xa[1];
	double xaz = xa[2];
	double xbx = xb[0];
	double xby = xb[1];
	double xbz = xb[2];
	double xcx = xc[0];
	double xcy = xc[1];
	double xcz = xc[2];
	double Xax = Xa[0];
	double Xay = Xa[1];
	double Xbx = Xb[0];
	double Xby = Xb[1];
	double Xcx = Xc[0];
	double Xcy = Xc[1];

	double P00 = P(0, 0);
	double P10 = P(1, 0);
	double P01 = P(0, 1);
	double P11 = P(1, 1);
	double P02 = P(0, 2);
	double P12 = P(1, 2);
	double Q00 = Q(0, 0);
	double Q10 = Q(1, 0);
	double Q01 = Q(0, 1);
	double Q11 = Q(1, 1);

	double t7 = Xax * Xby - Xax * Xcy - Xbx * Xay + Xcx * Xay + Xbx * Xcy - Xcx * Xby;
	double t8 = t7 / 2;
	double t9 = 1 + nu;
	double t12 = e / t9 / 2;
	double t15 = P00 * Q00 + P10 * Q10;
	double t17 = 0.1e1 / t7;
	double t18 = (xbx - xax) * t17;
	double t19 = Xcy - Xay;
	double t22 = (xcx - xax) * t17;
	double t23 = -Xby + Xay;
	double t25 = t18 * t19 + t22 * t23;
	double t26 = t15 * t25;
	double t29 = P01 * Q00 + P11 * Q10;
	double t31 = (xby - xay) * t17;
	double t34 = (xcy - xay) * t17;
	double t36 = t31 * t19 + t34 * t23;
	double t37 = t29 * t36;
	double t40 = P02 * Q00 + P12 * Q10;
	double t42 = (xbz - xaz) * t17;
	double t45 = (xcz - xaz) * t17;
	double t47 = t42 * t19 + t45 * t23;
	double t48 = t40 * t47;
	double t49 = t26 + t37 + t48 - 1;
	double t50 = t49 * t49;
	double t53 = P00 * Q01 + P10 * Q11;
	double t57 = P01 * Q01 + P11 * Q11;
	double t61 = P02 * Q01 + P12 * Q11;
	double t63 = t53 * t25 + t57 * t36 + t61 * t47;
	double t64 = t63 * t63;
	double t65 = -Xcx + Xax;
	double t67 = Xbx - Xax;
	double t69 = t18 * t65 + t22 * t67;
	double t73 = t31 * t65 + t34 * t67;
	double t77 = t42 * t65 + t45 * t67;
	double t79 = t15 * t69 + t29 * t73 + t40 * t77;
	double t80 = t79 * t79;
	double t81 = t53 * t69;
	double t82 = t57 * t73;
	double t83 = t61 * t77;
	double t84 = t81 + t82 + t83 - 1;
	double t85 = t84 * t84;
	double t88 = e * nu;
	double t89 = 1 / t9;
	double t92 = 1 / (1 - 2 * nu);
	double t93 = t89 * t92;
	double t94 = t26 + t37 + t48 - 2 + t81 + t82 + t83;
	double t95 = t94 * t94;
	double W00 = t8 * (t12 * (t50 + t64 + t80 + t85) + t88 * t93 * t95 / 2);
	double t100 = t49 * t15;
	double t101 = t17 * t19;
	double t102 = t17 * t23;
	double t103 = -t101 - t102;
	double t105 = t63 * t53;
	double t107 = t79 * t15;
	double t108 = t17 * t65;
	double t109 = t17 * t67;
	double t110 = -t108 - t109;
	double t112 = t84 * t53;
	double t117 = t88 * t89;
	double t118 = t92 * t94;
	double t121 = t15 * t103 + t53 * t110;
	double f01 = -t8 * (t12 * (2 * t100 * t103 + 2 * t105 * t103 + 2 * t107 * t110 + 2 * t112 * t110) + t117 * t118 * t121);
	double t126 = t49 * t29;
	double t128 = t63 * t57;
	double t130 = t79 * t29;
	double t132 = t84 * t57;
	double t139 = t29 * t103 + t57 * t110;
	double f02 = -t8 * (t12 * (2 * t126 * t103 + 2 * t128 * t103 + 2 * t130 * t110 + 2 * t132 * t110) + t117 * t118 * t139);
	double t144 = t49 * t40;
	double t146 = t63 * t61;
	double t148 = t79 * t40;
	double t150 = t84 * t61;
	double t157 = t40 * t103 + t61 * t110;
	double f03 = -t8 * (t12 * (2 * t144 * t103 + 2 * t146 * t103 + 2 * t148 * t110 + 2 * t150 * t110) + t117 * t118 * t157);
	double t169 = t15 * t17;
	double t171 = t53 * t17;
	double t173 = t169 * t19 + t171 * t65;
	double f04 = -t8 * (t12 * (2 * t100 * t101 + 2 * t105 * t101 + 2 * t107 * t108 + 2 * t112 * t108) + t117 * t118 * t173);
	double t185 = t29 * t17;
	double t187 = t57 * t17;
	double t189 = t185 * t19 + t187 * t65;
	double f05 = -t8 * (t12 * (2 * t126 * t101 + 2 * t128 * t101 + 2 * t130 * t108 + 2 * t132 * t108) + t117 * t118 * t189);
	double t201 = t40 * t17;
	double t203 = t61 * t17;
	double t205 = t201 * t19 + t203 * t65;
	double f06 = -t8 * (t12 * (2 * t144 * t101 + 2 * t146 * t101 + 2 * t148 * t108 + 2 * t150 * t108) + t117 * t118 * t205);
	double t219 = t169 * t23 + t171 * t67;
	double f07 = -t8 * (t12 * (2 * t100 * t102 + 2 * t105 * t102 + 2 * t107 * t109 + 2 * t112 * t109) + t117 * t118 * t219);
	double t233 = t185 * t23 + t187 * t67;
	double f08 = -t8 * (t12 * (2 * t126 * t102 + 2 * t128 * t102 + 2 * t130 * t109 + 2 * t132 * t109) + t117 * t118 * t233);
	double t247 = t201 * t23 + t203 * t67;
	double f09 = -t8 * (t12 * (2 * t144 * t102 + 2 * t146 * t102 + 2 * t148 * t109 + 2 * t150 * t109) + t117 * t118 * t247);
	double t252 = t15 * t15;
	double t253 = t103 * t103;
	double t255 = t53 * t53;
	double t257 = t110 * t110;
	double t263 = t121 * t121;
	double K0101 = t8 * (t12 * (2 * t252 * t253 + 2 * t252 * t257 + 2 * t255 * t253 + 2 * t255 * t257) + t88 * t93 * t263);
	double K0102 = t8 * (t12 * (2 * t29 * t253 * t15 + 2 * t29 * t257 * t15 + 2 * t57 * t253 * t53 + 2 * t57 * t257 * t53) + t117 * t92 * t139 * t121);
	double t282 = t40 * t253;
	double t284 = t61 * t253;
	double t286 = t40 * t257;
	double t288 = t61 * t257;
	double t293 = t92 * t157;
	double K0103 = t8 * (t12 * (2 * t282 * t15 + 2 * t286 * t15 + 2 * t284 * t53 + 2 * t288 * t53) + t117 * t293 * t121);
	double t297 = t252 * t17;
	double t298 = t19 * t103;
	double t300 = t255 * t17;
	double t302 = t65 * t110;
	double t308 = t92 * t173;
	double K0104 = t8 * (t12 * (2 * t297 * t298 + 2 * t297 * t302 + 2 * t300 * t298 + 2 * t300 * t302) + t117 * t308 * t121);
	double t313 = t19 * t15 * t103;
	double t316 = t19 * t53 * t103;
	double t319 = t65 * t15 * t110;
	double t322 = t65 * t53 * t110;
	double t326 = t12 * (2 * t185 * t313 + 2 * t185 * t319 + 2 * t187 * t316 + 2 * t187 * t322);
	double t327 = t92 * t189;
	double K0105 = t8 * (t117 * t327 * t121 + t326);
	double t337 = t12 * (2 * t201 * t313 + 2 * t201 * t319 + 2 * t203 * t316 + 2 * t203 * t322);
	double t338 = t92 * t205;
	double K0106 = t8 * (t117 * t338 * t121 + t337);
	double t342 = t23 * t103;
	double t345 = t67 * t110;
	double t351 = t92 * t219;
	double K0107 = t8 * (t12 * (2 * t297 * t342 + 2 * t297 * t345 + 2 * t300 * t342 + 2 * t300 * t345) + t117 * t351 * t121);
	double t355 = t23 * t15;
	double t356 = t355 * t103;
	double t358 = t23 * t53;
	double t359 = t358 * t103;
	double t361 = t67 * t15;
	double t362 = t361 * t110;
	double t364 = t67 * t53;
	double t365 = t364 * t110;
	double t369 = t12 * (2 * t185 * t356 + 2 * t185 * t362 + 2 * t187 * t359 + 2 * t187 * t365);
	double t370 = t92 * t233;
	double K0108 = t8 * (t117 * t370 * t121 + t369);
	double t380 = t12 * (2 * t201 * t356 + 2 * t201 * t362 + 2 * t203 * t359 + 2 * t203 * t365);
	double t381 = t92 * t247;
	double K0109 = t8 * (t117 * t381 * t121 + t380);
	double K0201 = K0102;
	double t385 = t29 * t29;
	double t387 = t57 * t57;
	double t394 = t139 * t139;
	double K0202 = t8 * (t12 * (2 * t385 * t253 + 2 * t387 * t253 + 2 * t385 * t257 + 2 * t387 * t257) + t88 * t93 * t394);
	double K0203 = t8 * (t12 * (2 * t282 * t29 + 2 * t284 * t57 + 2 * t286 * t29 + 2 * t288 * t57) + t117 * t293 * t139);
	double K0204 = t8 * (t117 * t308 * t139 + t326);
	double t411 = t385 * t17;
	double t413 = t387 * t17;
	double K0205 = t8 * (t12 * (2 * t411 * t298 + 2 * t413 * t298 + 2 * t411 * t302 + 2 * t413 * t302) + t117 * t327 * t139);
	double t437 = t12 * (2 * t201 * t19 * t29 * t103 + 2 * t203 * t19 * t57 * t103 + 2 * t201 * t65 * t29 * t110 + 2 * t203 * t65 * t57 * t110);
	double K0206 = t8 * (t117 * t338 * t139 + t437);
	double K0207 = t8 * (t117 * t351 * t139 + t369);
	double K0208 = t8 * (t12 * (2 * t411 * t342 + 2 * t413 * t342 + 2 * t411 * t345 + 2 * t413 * t345) + t117 * t370 * t139);
	double t454 = t23 * t29;
	double t457 = t23 * t57;
	double t460 = t67 * t29;
	double t463 = t67 * t57;
	double t468 = t12 * (2 * t201 * t454 * t103 + 2 * t203 * t457 * t103 + 2 * t201 * t460 * t110 + 2 * t203 * t463 * t110);
	double K0209 = t8 * (t117 * t381 * t139 + t468);
	double K0301 = K0103;
	double K0302 = K0203;
	double t472 = t40 * t40;
	double t474 = t61 * t61;
	double t481 = t157 * t157;
	double K0303 = t8 * (t12 * (2 * t472 * t253 + 2 * t474 * t253 + 2 * t472 * t257 + 2 * t474 * t257) + t88 * t93 * t481);
	double K0304 = t8 * (t117 * t308 * t157 + t337);
	double K0305 = t8 * (t117 * t327 * t157 + t437);
	double t491 = t472 * t17;
	double t493 = t474 * t17;
	double K0306 = t8 * (t12 * (2 * t491 * t298 + 2 * t493 * t298 + 2 * t491 * t302 + 2 * t493 * t302) + t117 * t338 * t157);
	double K0307 = t8 * (t117 * t351 * t157 + t380);
	double K0308 = t8 * (t117 * t370 * t157 + t468);
	double K0309 = t8 * (t12 * (2 * t491 * t342 + 2 * t493 * t342 + 2 * t491 * t345 + 2 * t493 * t345) + t117 * t381 * t157);
	double K0401 = K0104;
	double K0402 = K0204;
	double K0403 = K0304;
	double t519 = t7 * t7;
	double t520 = 0.1e1 / t519;
	double t521 = t252 * t520;
	double t522 = t19 * t19;
	double t524 = t255 * t520;
	double t526 = t65 * t65;
	double t532 = t173 * t173;
	double K0404 = t8 * (t12 * (2 * t521 * t522 + 2 * t521 * t526 + 2 * t524 * t522 + 2 * t524 * t526) + t88 * t93 * t532);
	double t536 = t29 * t520;
	double t537 = t522 * t15;
	double t539 = t57 * t520;
	double t540 = t522 * t53;
	double t542 = t526 * t15;
	double t544 = t526 * t53;
	double K0405 = t8 * (t12 * (2 * t536 * t537 + 2 * t536 * t542 + 2 * t539 * t540 + 2 * t539 * t544) + t117 * t327 * t173);
	double t552 = t40 * t520;
	double t554 = t61 * t520;
	double K0406 = t8 * (t12 * (2 * t552 * t537 + 2 * t554 * t540 + 2 * t552 * t542 + 2 * t554 * t544) + t117 * t338 * t173);
	double t564 = t23 * t19;
	double t567 = t67 * t65;
	double K0407 = t8 * (t12 * (2 * t521 * t564 + 2 * t521 * t567 + 2 * t524 * t564 + 2 * t524 * t567) + t117 * t351 * t173);
	double t576 = t355 * t19;
	double t578 = t358 * t19;
	double t580 = t361 * t65;
	double t582 = t364 * t65;
	double t586 = t12 * (2 * t536 * t576 + 2 * t536 * t580 + 2 * t539 * t578 + 2 * t539 * t582);
	double K0408 = t8 * (t117 * t370 * t173 + t586);
	double t596 = t12 * (2 * t552 * t576 + 2 * t552 * t580 + 2 * t554 * t578 + 2 * t554 * t582);
	double K0409 = t8 * (t117 * t381 * t173 + t596);
	double K0501 = K0105;
	double K0502 = K0205;
	double K0503 = K0305;
	double K0504 = K0405;
	double t600 = t385 * t520;
	double t602 = t387 * t520;
	double t609 = t189 * t189;
	double K0505 = t8 * (t12 * (2 * t600 * t522 + 2 * t602 * t522 + 2 * t600 * t526 + 2 * t602 * t526) + t88 * t93 * t609);
	double K0506 = t8 * (t12 * (2 * t552 * t522 * t29 + 2 * t552 * t526 * t29 + 2 * t554 * t522 * t57 + 2 * t554 * t526 * t57) + t117 * t338 * t189);
	double K0507 = t8 * (t117 * t351 * t189 + t586);
	double K0508 = t8 * (t12 * (2 * t600 * t564 + 2 * t602 * t564 + 2 * t600 * t567 + 2 * t602 * t567) + t117 * t370 * t189);
	double t650 = t12 * (2 * t552 * t454 * t19 + 2 * t554 * t457 * t19 + 2 * t552 * t460 * t65 + 2 * t554 * t463 * t65);
	double K0509 = t8 * (t117 * t381 * t189 + t650);
	double K0601 = K0106;
	double K0602 = K0206;
	double K0603 = K0306;
	double K0604 = K0406;
	double K0605 = K0506;
	double t654 = t472 * t520;
	double t656 = t474 * t520;
	double t663 = t205 * t205;
	double K0606 = t8 * (t12 * (2 * t654 * t522 + 2 * t656 * t522 + 2 * t654 * t526 + 2 * t656 * t526) + t88 * t93 * t663);
	double K0607 = t8 * (t117 * t351 * t205 + t596);
	double K0608 = t8 * (t117 * t370 * t205 + t650);
	double K0609 = t8 * (t12 * (2 * t654 * t564 + 2 * t656 * t564 + 2 * t654 * t567 + 2 * t656 * t567) + t117 * t381 * t205);
	double K0701 = K0107;
	double K0702 = K0207;
	double K0703 = K0307;
	double K0704 = K0407;
	double K0705 = K0507;
	double K0706 = K0607;
	double t683 = t23 * t23;
	double t686 = t67 * t67;
	double t692 = t219 * t219;
	double K0707 = t8 * (t12 * (2 * t521 * t683 + 2 * t521 * t686 + 2 * t524 * t683 + 2 * t524 * t686) + t88 * t93 * t692);
	double t696 = t683 * t15;
	double t698 = t683 * t53;
	double t700 = t686 * t15;
	double t702 = t686 * t53;
	double K0708 = t8 * (t12 * (2 * t536 * t696 + 2 * t536 * t700 + 2 * t539 * t698 + 2 * t539 * t702) + t117 * t370 * t219);
	double K0709 = t8 * (t12 * (2 * t552 * t696 + 2 * t552 * t700 + 2 * t554 * t698 + 2 * t554 * t702) + t117 * t381 * t219);
	double K0801 = K0108;
	double K0802 = K0208;
	double K0803 = K0308;
	double K0804 = K0408;
	double K0805 = K0508;
	double K0806 = K0608;
	double K0807 = K0708;
	double t727 = t233 * t233;
	double K0808 = t8 * (t12 * (2 * t600 * t683 + 2 * t600 * t686 + 2 * t602 * t683 + 2 * t602 * t686) + t88 * t93 * t727);
	double K0809 = t8 * (t12 * (2 * t552 * t683 * t29 + 2 * t552 * t686 * t29 + 2 * t554 * t683 * t57 + 2 * t554 * t686 * t57) + t117 * t381 * t233);
	double K0901 = K0109;
	double K0902 = K0209;
	double K0903 = K0309;
	double K0904 = K0409;
	double K0905 = K0509;
	double K0906 = K0609;
	double K0907 = K0709;
	double K0908 = K0809;
	double t752 = t247 * t247;
	double K0909 = t8 * (t12 * (2 * t654 * t683 + 2 * t654 * t686 + 2 * t656 * t683 + 2 * t656 * t686) + t88 * t93 * t752);

	W = W00;
	f(0) = f01; f(1) = f02; f(2) = f03; f(3) = f04; f(4) = f05; f(5) = f06; f(6) = f07; f(7) = f08; f(8) = f09;

	K << K0101, K0102, K0103, K0104, K0105, K0106, K0107, K0108, K0109,
		K0201, K0202, K0203, K0204, K0205, K0206, K0207, K0208, K0209,
		K0301, K0302, K0303, K0304, K0305, K0306, K0307, K0308, K0309,
		K0401, K0402, K0403, K0404, K0405, K0406, K0407, K0408, K0409,
		K0501, K0502, K0503, K0504, K0505, K0506, K0507, K0508, K0509,
		K0601, K0602, K0603, K0604, K0605, K0606, K0607, K0608, K0609,
		K0701, K0702, K0703, K0704, K0705, K0706, K0707, K0708, K0709,
		K0801, K0802, K0803, K0804, K0805, K0806, K0807, K0808, K0809,
		K0901, K0902, K0903, K0904, K0905, K0906, K0907, K0908, K0909;
}
