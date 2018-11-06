#include <cuda.h>
#include <curand.h>
#include <set>
#include <list>
#include <vector>
#include <memory>
#include "NodeSystemBuilder.h"
#include "NodeSystemDevice.h"
# define M_PI 3.14159265358979323846  /* pi */


NodeSystemBuilder::NodeSystemBuilder(double _epsilon, double _dt):
	epsilon(_epsilon), dt(_dt) {}

NodeSystemBuilder::~NodeSystemBuilder() {

}


//currently unused
void NodeSystemBuilder::putLinearSpring(__attribute__ ((unused)) unsigned n1,__attribute__ ((unused)) unsigned n2) {

	//auto p1 = model->getNode(n1);
	//auto p2 = model->getNode(n2);
	//double length = glm::length(nodePositions[n1] - nodePositions[n2]);	//preferred length set


}

void NodeSystemBuilder::putWormLikeSpring(unsigned n1, unsigned n2) {


	double lengthZero = glm::length(nodePositions[n1] - nodePositions[n2]);

	//store for parallel solving.
	hostWLCLenZero.push_back(lengthZero);
	hostWLCEdgeLeft.push_back(n1);
	hostWLCEdgeRight.push_back(n2);

	//std::cout << "putting wlc spring between " << n1 << " and " << n2 << std::endl;
	//std::cout << "length " << lengthZero << std::endl;
}

void NodeSystemBuilder::putSpring(unsigned n1, unsigned n2) {

	return putWormLikeSpring(n1, n2);
}

void NodeSystemBuilder::putSubSpring(unsigned n1, unsigned n2) {

	double lengthZero = glm::length(nodePositions[n1] - nodePositions[n2]);

	//store for parallel solving.
	hostWLCLenSubZero.push_back(lengthZero);
	hostWLCEdgeSubLeft.push_back(n1);
	hostWLCEdgeSubRight.push_back(n2);
	//std::cout << "putting wlc spring between " << n1 << " and " << n2 << std::endl;
	//std::cout << "length " << lengthZero << std::endl;
}

void NodeSystemBuilder::putTorsionSpring(unsigned n1, unsigned n2, unsigned n3) {

	glm::dvec3 p1 = nodePositions[n1] - nodePositions[n2];
	glm::dvec3 p2 = nodePositions[n3] - nodePositions[n2];

	double cos_theta = glm::dot(p1, p2) / (glm::length(p1) * glm::length(p2));
	double preferredAngle;
	if (cos_theta > 1) {
		cos_theta = 1.0;
	}
	else if (cos_theta < -1) {
		cos_theta = -1.0;
	}

	preferredAngle = std::acos(cos_theta);


	hostTorsionEdgeLeft.push_back(n1);
	hostTorsionEdgeCenter.push_back(n2);
	hostTorsionEdgeRight.push_back(n3);
	hostTorsionAngleZero.push_back(preferredAngle);

}





//all node pointers made here. We take this opportunity to
unsigned NodeSystemBuilder::addNode(__attribute__ ((unused)) double mass, glm::dvec3 pos ) {

	unsigned newId = buildNodes.size();
	//notice the node and buildnode have the same corresponding id's.
	std::shared_ptr<BuildNode> ptr1(new BuildNode(newId));
	buildNodes.push_back(ptr1);

	nodePositions.push_back(pos);

	hostNodeIds.push_back(newId);

	hostPosX.push_back(pos.x);
	hostPosY.push_back(pos.y);
	hostPosZ.push_back(pos.z);



	hostIsNodeFixed.push_back(false);
	return newId;

}

//add platelets
//all node pointers made here. We take this opportunity to
unsigned NodeSystemBuilder::addPlt(__attribute__ ((unused)) double pltmass, glm::dvec3 pos ) {

	unsigned newId = buildPlts.size();
	//notice the node and buildnode have the same corresponding id's.
	std::shared_ptr<BuildPlt> ptr1(new BuildPlt(newId));
	buildPlts.push_back(ptr1);

	pltPositions.push_back(pos);

	hostPltIds.push_back(newId);

	hostPltPosX.push_back(pos.x);
	hostPltPosY.push_back(pos.y);
	hostPltPosZ.push_back(pos.z);



	hostIsPltFixed.push_back(false);
	return newId;

}



//list holds positions of subnodes
std::list<glm::dvec3> NodeSystemBuilder::fillSpace(glm::dvec3 from, glm::dvec3 to, unsigned subNodes) {

	std::list<glm::dvec3> list;
	glm::dvec3 segment = (to - from) / (subNodes + 1.0);
	for (unsigned i = 1; i <= subNodes; ++i)
		list.push_back(from + segment * (double) i);
	return list;
}



//this method is made to fix an error in how the buildNodes are generated.
//Each node's neighbors are scanned, and depending on how many there are, a different number of springs
//is added.
//notice that we sort the neighbors so torsion springs will be placed in a specific ordering. This is used when preferred angles are stored in a matrix.
void NodeSystemBuilder::generateBuildNodesTriplets() {
	for (unsigned i = 0; i < nodePositions.size(); i++) {
		auto ptrBN = buildNodes[i];
		unsigned center = ptrBN->id;

		//find neighbors of center
		std::vector<unsigned> neighbors;
		for (unsigned i = 0; i < hostWLCEdgeLeft.size(); ++i) {
			unsigned idLeft = hostWLCEdgeLeft[i];
			unsigned idRight = hostWLCEdgeRight[i];
			if (idLeft == center) {
				neighbors.push_back(idRight);
			}
			if (idRight == center) {
				neighbors.push_back(idLeft);
			}
		}


		//now that we have the neighbors, we'll add pairs to prev and next
		//we'll sort them before adding.

		std::sort(neighbors.begin(), neighbors.end());

		//for the buildNode related to center
		//we only continue if there are more than one neighbor.
		//if two neightbors, we need one torsion spring
		if (neighbors.size() == 2) {
			ptrBN->prev.push_back(neighbors[0]);
			ptrBN->next.push_back(neighbors[1]);
			continue;
		}

		//if n>2 neighbors we need n torsion springs
		if (neighbors.size() > 2) {
			for (unsigned j = 0; j < neighbors.size(); ++j) {

				ptrBN->prev.push_back(neighbors[j]);
				unsigned index = (j + 1) % neighbors.size();
				ptrBN->next.push_back(neighbors[index]);
			}
		}

	}
}

void NodeSystemBuilder::fixNode(unsigned id) {
	hostIsNodeFixed[id] = true;
}

//fix platelet
void NodeSystemBuilder::fixPlt(unsigned id) {
	hostIsPltFixed[id] = true;
}

void NodeSystemBuilder::addSubnodes() {
	std::cout << "setting subnodes. Total edges: "<< hostWLCEdgeLeft.size() << std::endl;
	//if use Extra nodes is true, we complete this section
	if (useExtraNodes) {
		//std::cout << "use extra nodes: " << useExtraNodes << std::endl;
		for (unsigned i = 0; i < hostWLCEdgeLeft.size(); ++i) {

			unsigned idLeft = hostWLCEdgeLeft[i];
			unsigned idRight = hostWLCEdgeRight[i];

			unsigned subNodeCount = 0; //default subnode amount

			if (useConstantNumberOfExtraNodes) {
				subNodeCount = defaultExtraNodesPerEdge;
			}
			else {
				double length = glm::length(nodePositions[idLeft] - nodePositions[idRight]);
				subNodeCount = (unsigned)glm::round(length / defaultUnitsPerExtraNode);

				//if we are not using a constant number of nodes, make the variable
				//hold the largest amount of subnodes per edge.
				if (subNodeCount > defaultExtraNodesPerEdge) {
					defaultExtraNodesPerEdge = subNodeCount;
				}


			}

			if (subNodeCount == 0)
				std::cout << "subnodes cannot be placed" << "\n";

			// fill space between two nodes by adding a number of extra nodes
			// you can choose these or they will be rounded.
			auto points = fillSpace(nodePositions[idLeft], nodePositions[idRight], subNodeCount);
			std::vector<unsigned> subNodeIds;

			//notice that each subnode is added to the vector of no des
			for (glm::dvec3& point : points) {
				//std::cout << "sn:" << point.x << " " << point.y << "\n";
				subNodeIds.push_back(addNode(defaultSubNodeMass, point));
			}

			if (subNodeCount == 0)
			{
				std::cout << "subnodesCount = 0 after fillSpace" << "\n";
				continue;
			}

			//link head
			putSubSpring(idLeft, subNodeIds.front());
			// link all nodes to make a chain
			for (unsigned i = 0; i < subNodeIds.size() - 1; ++i) {
				putSubSpring(subNodeIds[i], subNodeIds[i + 1]);
			}
			// link main nodes with and tail of list of sub-nodes

			putSubSpring(subNodeIds.back(), idRight);


		}

	}

}

//adds all constraints to the nodesystem model so that it can use the constraints.
std::shared_ptr<NodeSystemDevice> NodeSystemBuilder::create() {
	//before adding subnodes generate original numbers of nodes and links
	originLinkCount = hostWLCEdgeLeft.size();
	originNodeCount = hostPosX.size();
	//platelet counts
	originPltCount = hostPltPosX.size();
	//first add all subnodes
	addSubnodes();
	std::cout << "edge count: " << hostWLCLenZero.size() << std::endl;
	if (hostWLCEdgeSubLeft.size() > hostWLCEdgeLeft.size()) {
		//if true, we have added subnodes and need to update the input files.
		hostWLCEdgeLeft = hostWLCEdgeSubLeft;
		hostWLCEdgeRight = hostWLCEdgeSubRight;
		hostWLCLenZero = hostWLCLenSubZero;
	}

	std::cout << "edge count: " << hostWLCLenZero.size() << std::endl;

	numNodes = hostPosX.size();
	numPlts = hostPltPosX.size();
	numEdges = hostWLCEdgeLeft.size();
	std::cout << "node count: " << numNodes << std::endl;//Plt
	std::cout << "platelet count: " << numPlts << std::endl;


	//preferred edge angle is always taken into account.
	std::cout << "Torsion springs mounting" << std::endl;
	generateBuildNodesTriplets(); //sets all nodes in buildNodes neighbors for linking in next 10 lines
	for (auto& center : buildNodes) {
		for (unsigned i = 0; i < (center->next).size(); ++i) {
			unsigned left = center->prev[i];
			unsigned right = center->next[i];
			putTorsionSpring(left, center->id, right);//set theta value here

		}
	}


	//now all the edges and variables are set.
	//so set the system and return a pointer.
	std::shared_ptr<NodeSystemDevice> host_ptr_devNodeSystem = std::make_shared<NodeSystemDevice>();

	host_ptr_devNodeSystem->generalParams.maxNodeCount = hostPosX.size();
	host_ptr_devNodeSystem->generalParams.maxPltCount = hostPltPosX.size();//Plt
	host_ptr_devNodeSystem->generalParams.totalTorsionCount = hostTorsionAngleZero.size();

	host_ptr_devNodeSystem->generalParams.originNodeCount = originNodeCount;
	host_ptr_devNodeSystem->generalParams.originPltCount = originPltCount;//Plt


	host_ptr_devNodeSystem->generalParams.originNodeCount = originNodeCount;//double?
	host_ptr_devNodeSystem->generalParams.originLinkCount = originLinkCount;
	host_ptr_devNodeSystem->generalParams.originEdgeCount = numEdges;
	host_ptr_devNodeSystem->generalParams.subNodeCount = defaultExtraNodesPerEdge;
	host_ptr_devNodeSystem->generalParams.epsilon = epsilon;
	host_ptr_devNodeSystem->generalParams.dtTemp = dt;
	host_ptr_devNodeSystem->generalParams.kB = defaultBoltzmannConstant;
	host_ptr_devNodeSystem->generalParams.CLM = defaultContourLengthMultiplier;
	host_ptr_devNodeSystem->generalParams.torsionStiffness = defaultTorsionSpringStiffness;
	host_ptr_devNodeSystem->generalParams.viscousDamp = defaultResistance;
	host_ptr_devNodeSystem->generalParams.temperature = defaultTemperature;
	host_ptr_devNodeSystem->generalParams.persistenceLengthMon = defaultPersistanceLength;
	host_ptr_devNodeSystem->generalParams.nodeMass = defaultMass;


	host_ptr_devNodeSystem->generalParams.linking = linking;

	std::cout<< host_ptr_devNodeSystem->generalParams.kB<< " "<< host_ptr_devNodeSystem->generalParams.temperature << " "<< host_ptr_devNodeSystem->generalParams.torsionStiffness<< " "<< host_ptr_devNodeSystem->generalParams.persistenceLengthMon<<std::endl;

	std::cout<<"default res: "<< defaultResistance << std::endl;
	std::cout<< host_ptr_devNodeSystem->generalParams.dtTemp<< " "<< host_ptr_devNodeSystem->generalParams.df << " "<< host_ptr_devNodeSystem->generalParams.viscousDamp<<std::endl;

	//platelet parameters
	host_ptr_devNodeSystem->generalParams.pltForce = pltForce;
	host_ptr_devNodeSystem->generalParams.pltMaxConn = pltMaxConn;
	host_ptr_devNodeSystem->generalParams.pltR = pltR;
	host_ptr_devNodeSystem->generalParams.pltRForce = pltRForce;
	host_ptr_devNodeSystem->generalParams.pltMass = defaultPltMass;
	host_ptr_devNodeSystem->domainParams.gridSpacing = std::max(pltRForce, 5*defaultLinkDiameter);
	host_ptr_devNodeSystem->generalParams.fiberDiameter = defaultLinkDiameter ;

	host_ptr_devNodeSystem->initializeSystem(
		hostIsNodeFixed,
		hostPosX,
		hostPosY,
		hostPosZ,
		hostWLCEdgeLeft,
		hostWLCEdgeRight,
		hostWLCLenZero,

		hostWLCEdgeSubLeft,
		hostWLCEdgeSubRight,
		hostWLCLenSubZero,
		hostTorsionEdgeLeft,
		hostTorsionEdgeCenter,
		hostTorsionEdgeRight,
		hostTorsionAngleZero,
//platelets
		hostIsPltFixed,
		hostPltPosX,
		hostPltPosY,
		hostPltPosZ

	);






	return host_ptr_devNodeSystem;

}