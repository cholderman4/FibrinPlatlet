
#ifndef PLATELETFORCEDEVICE_H_
#define PLATELETFORCEDEVICE_H_

#include <vector>
#include "SystemStructures.h"



void PltForceOnDevice(
  NodeInfoVecs& nodeInfoVecs,
	WLCInfoVecs& wlcInfoVecs,
	GeneralParams& generalParams,
  PltInfoVecs& pltInfoVecs,
  AuxVecs& auxVecs);



struct AddPltonNodeForceFunctor {//same as torsion
	double* forceXAddr;
	double* forceYAddr;
	double* forceZAddr;

	__host__ __device__
	//
		AddPltonNodeForceFunctor(
				double* _forceXAddr,
				double* _forceYAddr,
				double* _forceZAddr) :
			forceXAddr(_forceXAddr),
			forceYAddr(_forceYAddr),
			forceZAddr(_forceZAddr) {}

	__device__
	void operator() (const Tuddd& u1d3) {
			unsigned idToAssign = thrust::get<0>(u1d3);
			if (!isnan(thrust::get<1>(u1d3)) && !isnan(thrust::get<2>(u1d3)) && !isnan(thrust::get<3>(u1d3))) {

			forceXAddr[idToAssign] += thrust::get<1>(u1d3);
			forceYAddr[idToAssign] += thrust::get<2>(u1d3);
			forceZAddr[idToAssign] += thrust::get<3>(u1d3);
			}

	}

};

//go through and add appropriate entries for input
struct PltonNodeForceFunctor : public thrust::unary_function<U2CVec3, CVec3>  {
  unsigned pltmaxConn;
  double pltRForce;
  double pltForce;
  double pltR;
  unsigned maxPltCount;

  double* nodeLocXAddr;
	double* nodeLocYAddr;
	double* nodeLocZAddr;
	double* nodeUForceXAddr;
	double* nodeUForceYAddr;
	double* nodeUForceZAddr;
  unsigned* nodeUId;

	unsigned* bucketNbrsExp;
	unsigned* keyBegin;
	unsigned* keyEnd;


   __host__ __device__
   //
       PltonNodeForceFunctor(
            unsigned& _pltmaxConn,
            double& _pltRForce,
            double& _pltForce,
            double& _pltR,
            unsigned& _maxPltCount,

            double* _nodeLocXAddr,
            double* _nodeLocYAddr,
            double* _nodeLocZAddr,
            double* _nodeUForceXAddr,
            double* _nodeUForceYAddr,
            double* _nodeUForceZAddr,
      			unsigned* _nodeUId,

      			unsigned* _bucketNbrsExp,
      			unsigned* _keyBegin,
      			unsigned* _keyEnd) :

    pltmaxConn(_pltmaxConn),
    pltRForce(_pltRForce),
    pltForce(_pltForce),
    pltR(_pltR),
    maxPltCount(_maxPltCount),

    nodeLocXAddr(_nodeLocXAddr),
		nodeLocYAddr(_nodeLocYAddr),
		nodeLocZAddr(_nodeLocZAddr),
		nodeUForceXAddr(_nodeUForceXAddr),
		nodeUForceYAddr(_nodeUForceYAddr),
		nodeUForceZAddr(_nodeUForceZAddr),
    nodeUId(_nodeUId),

		bucketNbrsExp(_bucketNbrsExp),
		keyBegin(_keyBegin),
		keyEnd(_keyEnd) {}


   __device__
 		CVec3 operator()(const U2CVec3 &u2d3) {

        unsigned bucketId = thrust::get<0>(u2d3);
        unsigned pltId = thrust::get<1>(u2d3);

        //beginning and end of attempted interaction network nodes.
		    unsigned beginIndex = keyBegin[bucketId];
		    unsigned endIndex = keyEnd[bucketId];


        unsigned storageLocation = pltId * pltmaxConn;

        double pltLocX = thrust::get<2>(u2d3);;
        double pltLocY = thrust::get<3>(u2d3);;
        double pltLocZ = thrust::get<4>(u2d3);;


        double sumPltForceX = 0.0;
        double sumPltForceY = 0.0;
        double sumPltForceZ = 0.0;

        //Loop through the number of available neighbors for each plt.
        unsigned pullCounter=0;
        
        for(unsigned i = beginIndex; i < endIndex; i++) {

          //Choose a neighbor.
          unsigned pullNode_id = bucketNbrsExp[i];
          //
          //Get position of node
          double vecN_PX = pltLocX - nodeLocXAddr[pullNode_id];
          double vecN_PY = pltLocY - nodeLocYAddr[pullNode_id];
          double vecN_PZ = pltLocZ - nodeLocZAddr[pullNode_id];
          //Calculate distance from plt to node.
          double dist = sqrt(
              (vecN_PX) * (vecN_PX) +
              (vecN_PY) * (vecN_PY) +
              (vecN_PZ) * (vecN_PZ));


          //only pull as many as are arms.
          if (pullCounter < pltmaxConn) {

              if ((dist < pltRForce) && (dist > pltR) ) {
                  //node only affects plt position if it is pulled.
                  //Determine direction of force based on positions and multiply magnitude force
                  double forceNodeX = (vecN_PX / dist) * (pltForce);
                  double forceNodeY = (vecN_PY / dist) * (pltForce);
                  double forceNodeZ = (vecN_PZ / dist) * (pltForce);

                  //count force for plt.
                  sumPltForceX += (-1.0) * forceNodeX;
                  sumPltForceY += (-1.0) * forceNodeY;
                  sumPltForceZ += (-1.0) * forceNodeZ;

                  //store force in temporary vector. Call reduction later.
                  nodeUForceXAddr[storageLocation + pullCounter] = forceNodeX;
                  nodeUForceXAddr[storageLocation + pullCounter] = forceNodeY;
                  nodeUForceXAddr[storageLocation + pullCounter] = forceNodeZ;
                  nodeUId[storageLocation + pullCounter] = pullNode_id;

                  pullCounter++;

              }
          }


    }




    return thrust::make_tuple(sumPltForceX, sumPltForceY, sumPltForceZ);


   }
};
#endif /*PLATELETFORCEDEVICE_H_*/