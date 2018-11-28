#ifndef PLTVLMPUSHDEVICE_H_
#define PLTVLMPUSHDEVICE_H_

#include <vector>
#include "SystemStructures.h"

void PltVlmPushOnDevice(
  NodeInfoVecs& nodeInfoVecs,
	WLCInfoVecs& wlcInfoVecs,
	GeneralParams& generalParams,
  PltInfoVecs& pltInfoVecs,
  AuxVecs& auxVecs);

  struct PltVlmPushForceFunctor : public thrust::unary_function< U2CVec6, CVec3 >  {
    unsigned plt_other_intrct;
    double pltRForce;
    double pltForce;
    double pltR;

    unsigned maxPltCount;
    double fiberDiameter;
    unsigned maxNodeCount;
    unsigned maxNeighborCount;

    double* nodeLocXAddr;
  	double* nodeLocYAddr;
  	double* nodeLocZAddr;
  	double* nodeUForceXAddr;
  	double* nodeUForceYAddr;
  	double* nodeUForceZAddr;

    unsigned* nodeUId;

  	unsigned* id_value_expanded;
  	unsigned* keyBegin;
  	unsigned* keyEnd;
    
  	unsigned* idPlt_value_expanded;
  	unsigned* keyPltBegin;
  	unsigned* keyPltEnd;

    double* pltLocXAddr;
  	double* pltLocYAddr;
  	double* pltLocZAddr;


     __host__ __device__
     //
         PltVlmPushForceFunctor(
              unsigned& _plt_other_intrct,
              double& _pltRForce,
              double& _pltForce,
              double& _pltR,

              unsigned& _maxPltCount,
              double& _fiberDiameter,
              unsigned& _maxNodeCount,
              unsigned& _maxNeighborCount,

              double* _nodeLocXAddr,
              double* _nodeLocYAddr,
              double* _nodeLocZAddr,
              double* _nodeUForceXAddr,
              double* _nodeUForceYAddr,
              double* _nodeUForceZAddr,
              
        			unsigned* _nodeUId,

        			unsigned* _id_value_expanded,
        			unsigned* _keyBegin,
        			unsigned* _keyEnd,
              
        			unsigned* _idPlt_value_expanded,
        			unsigned* _keyPltBegin,
        			unsigned* _keyPltEnd,

              double* _pltLocXAddr,
              double* _pltLocYAddr,
              double* _pltLocZAddr) :

      plt_other_intrct(_plt_other_intrct),
      pltRForce(_pltRForce),
      pltForce(_pltForce),
      pltR(_pltR),

      maxPltCount(_maxPltCount),
      fiberDiameter(_fiberDiameter),
      maxNodeCount(_maxNodeCount),
      maxNeighborCount(_maxNeighborCount),

      nodeLocXAddr(_nodeLocXAddr),
  		nodeLocYAddr(_nodeLocYAddr),
  		nodeLocZAddr(_nodeLocZAddr),
  		nodeUForceXAddr(_nodeUForceXAddr),
  		nodeUForceYAddr(_nodeUForceYAddr),
  		nodeUForceZAddr(_nodeUForceZAddr),

      nodeUId(_nodeUId),

  		id_value_expanded(_id_value_expanded),//these are for network
  		keyBegin(_keyBegin),
  		keyEnd(_keyEnd),

      idPlt_value_expanded(_idPlt_value_expanded),//these are for plt
  		keyPltBegin(_keyPltBegin),
  		keyPltEnd(_keyPltEnd),
      
      pltLocXAddr(_pltLocXAddr),
  		pltLocYAddr(_pltLocYAddr),
  		pltLocZAddr(_pltLocZAddr) {}


     __device__
   		CVec3 operator()(const U2CVec6 &u2d6) {

          unsigned pltId = thrust::get<0>(u2d6);
          unsigned bucketId = thrust::get<1>(u2d6);

          //beginning and end of attempted interaction network nodes.
  		    unsigned beginIndexNode = keyBegin[bucketId];
  		    unsigned endIndexNode = keyEnd[bucketId];

          unsigned beginIndexPlt = keyPltBegin[bucketId];
  		    unsigned endIndexPlt = keyPltEnd[bucketId];


          unsigned storageLocation = pltId * plt_other_intrct;

          double pltLocX = thrust::get<2>(u2d6);
          double pltLocY = thrust::get<3>(u2d6);
          double pltLocZ = thrust::get<4>(u2d6);
          
          //use for return. 
          double sumPltForceX = thrust::get<5>(u2d6);;
          double sumPltForceY = thrust::get<6>(u2d6);;
          double sumPltForceZ = thrust::get<7>(u2d6);;
        
          //pushing
          unsigned pushCounter = 0;

          //go through all nodes that might be pushed
          for( unsigned id_count = beginIndexNode; id_count < endIndexNode; id_count++){
              unsigned pushNode_id = id_value_expanded[id_count];
              
              if (pushCounter < plt_other_intrct) {//must be same as other counter.
                  
                  //Get position of node
                  double vecN_PX = pltLocX - nodeLocXAddr[pushNode_id];
                  double vecN_PY = pltLocY - nodeLocYAddr[pushNode_id];
                  double vecN_PZ = pltLocZ - nodeLocZAddr[pushNode_id];
                  //Calculate distance from plt to node.
                  double dist = sqrt(
                      (vecN_PX) * (vecN_PX) +
                      (vecN_PY) * (vecN_PY) +
                      (vecN_PZ) * (vecN_PZ));   

                  //check pushcounter
                      //repulsion if fiber and platelet overlap
                  if (dist < (pltR + fiberDiameter / 2.0) )  {
                      //node only affects plt position if it is pulled.
                      //Determine direction of force based on positions and multiply magnitude force
                      double forceNodeX = -(vecN_PX / dist) * (pltForce);
                      double forceNodeY = -(vecN_PY / dist) * (pltForce);
                      double forceNodeZ = -(vecN_PZ / dist) * (pltForce);   
                      //count force for plt.
                      sumPltForceX += (-1.0) * forceNodeX;
                      sumPltForceY += (-1.0) * forceNodeY;
                      sumPltForceZ += (-1.0) * forceNodeZ;    
                      //store force in temporary vector. Call reduction later.    
                      nodeUForceXAddr[storageLocation + pushCounter] = forceNodeX;
                      nodeUForceYAddr[storageLocation + pushCounter] = forceNodeY;
                      nodeUForceZAddr[storageLocation + pushCounter] = forceNodeZ;
                      nodeUId[storageLocation + pushCounter] = pushNode_id;
                      pushCounter++;
                  }
              }
          }

          //IN THIS SECTION THERE IS NOT WRITING TO VECTORS. NO NEED FOR INCREMENT
          //FORCE IS ONLY APPLIED TO SELF. 
          //go through all plts that might be pushed
          //only apply force to self. 
          for( unsigned i = beginIndexPlt; i < endIndexPlt; i++){
              unsigned pushPlt_id = idPlt_value_expanded[i];
              //
              //Get position of node
              double vecN_PX = pltLocX - pltLocXAddr[pushPlt_id];
              double vecN_PY = pltLocY - pltLocYAddr[pushPlt_id];
              double vecN_PZ = pltLocZ - pltLocZAddr[pushPlt_id];
              //Calculate distance from plt to node.
              double dist = sqrt(
                  (vecN_PX) * (vecN_PX) +
                  (vecN_PY) * (vecN_PY) +
                  (vecN_PZ) * (vecN_PZ));
  
  
              //check pushcounter
              //repulsion if fiber and platelet overlap
              if (dist < (2.0 * pltR ) )  {
                  //node only affects plt position if it is pulled.
                  //Determine direction of force based on positions and multiply magnitude force
                  double forcePltX = -(vecN_PX / dist) * (pltForce);
                  double forcePltY = -(vecN_PY / dist) * (pltForce);
                  double forcePltZ = -(vecN_PZ / dist) * (pltForce);
                  //count force for plt.
                  sumPltForceX += (-1.0) * forcePltX;
                  sumPltForceY += (-1.0) * forcePltY;
                  sumPltForceZ += (-1.0) * forcePltZ;
              }
          }
      //return platelet forces
      return thrust::make_tuple(sumPltForceX, sumPltForceY, sumPltForceZ);

     }
};

#endif