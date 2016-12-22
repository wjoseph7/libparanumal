#include <math.h>
#include <stdlib.h>
#include "mesh2D.h"

void boundaryConditions2D(iint bc, dfloat t, dfloat x, dfloat y,
			  dfloat uM, dfloat vM, dfloat pM,
			  dfloat *uP, dfloat *vP, dfloat *pP){
  if(bc==1){
    *uP = -uM;	
    *vP = -vM;	
    *pP = pM;	
  }		
  if(bc==2){	
    dfloat dx = 1.f/sqrt(2.f);
    dfloat dy = 1.f/sqrt(2.f);
    dfloat omega = 10.f*M_PI;
    dfloat wave = cos(omega*(t-(x*dx+y*dy)));	
    dfloat uI = dx*wave;
    dfloat vI = dy*wave;
    dfloat pI = wave;	
    *uP = -uM -2.f*uI;	
    *vP = -vM -2.f*vI;
    *pP = pM;		
  }
}


// function to compute surface contributions 
// for nodal DG acoustics right hand side
void meshAcousticsSurface2D(mesh2D *mesh, dfloat t){

  // temporary storage for flux terms
  dfloat *fluxu = (dfloat*) calloc(mesh->Nfp*mesh->Nfaces,sizeof(dfloat));
  dfloat *fluxv = (dfloat*) calloc(mesh->Nfp*mesh->Nfaces,sizeof(dfloat));
  dfloat *fluxp = (dfloat*) calloc(mesh->Nfp*mesh->Nfaces,sizeof(dfloat));

  // for all elements
  for(iint e=0;e<mesh->Nelements;++e){
    // for all face nodes of all elements
    for(iint n=0;n<mesh->Nfp*mesh->Nfaces;++n){
      // find face that owns this node
      iint face = n/mesh->Nfp;

      // load surface geofactors for this face
      iint sid = mesh->Nsgeo*(e*mesh->Nfaces+face);
      dfloat nx = mesh->sgeo[sid+0];
      dfloat ny = mesh->sgeo[sid+1];
      dfloat sJ = mesh->sgeo[sid+2];
      dfloat invJ = mesh->sgeo[sid+3];

      // indices of negative and positive traces of face node
      iint id  = e*mesh->Nfp*mesh->Nfaces + n;
      iint idM = mesh->vmapM[id];
      iint idP = mesh->vmapP[id];
      if(idP<0) idP=idM;
      iint qidM = mesh->Nfields*idM, qidP = mesh->Nfields*idP;
      
      // load negative trace node values of q
      dfloat uM = mesh->q[qidM+0];
      dfloat vM = mesh->q[qidM+1];
      dfloat pM = mesh->q[qidM+2];

      // load positive trace node values of q
      dfloat uP = mesh->q[qidP+0]; // fix BCs later
      dfloat vP = mesh->q[qidP+1];
      dfloat pP = mesh->q[qidP+2];

      // find boundary type
      iint boundaryType = mesh->EToB[e*mesh->Nfaces+face];
      if(boundaryType>0)
	boundaryConditions2D(boundaryType, t, mesh->x[idM], mesh->y[idM], uM, vM, pM, &uP, &vP, &pP);

      // compute (q^* - q^-)
      dfloat duS = 0.5f*(uP-uM) + mesh->Lambda2*(-nx)*(pP-pM);
      dfloat dvS = 0.5f*(vP-vM) + mesh->Lambda2*(-ny)*(pP-pM);
      dfloat dpS = 0.5f*(pP-pM) + mesh->Lambda2*(-nx*(uP-uM)-ny*(vP-vM));
      
      // evaluate "flux" terms: (sJ/J)*(A*nx+B*ny)*(q^* - q^-)
      fluxu[n] = invJ*sJ*(-nx*dpS);
      fluxv[n] = invJ*sJ*(-ny*dpS);
      fluxp[n] = invJ*sJ*(-nx*duS-ny*dvS);
    }

    // for each node in the element 
    for(iint n=0;n<mesh->Np;++n){
      iint id = mesh->Nfields*(mesh->Np*e + n);

      // load rhs data from volume fluxes
      dfloat rhsu = mesh->rhsq[id];
      dfloat rhsv = mesh->rhsq[id+1];
      dfloat rhsp = mesh->rhsq[id+2];
      
      // rhs += LIFT*((sJ/J)*(A*nx+B*ny)*(q^* - q^-))
      for(int m=0;m<mesh->Nfp*mesh->Nfaces;++m){
	dfloat L = mesh->LIFT[n*mesh->Nfp*mesh->Nfaces+m];
	rhsu += L*fluxu[m];
	rhsv += L*fluxv[m];
	rhsp += L*fluxp[m];
      }

      // store incremented rhs
      mesh->rhsq[id]   = rhsu;
      mesh->rhsq[id+1] = rhsv;
      mesh->rhsq[id+2] = rhsp;
    }
  }

  // free temporary flux storage
  free(fluxu); free(fluxv); free(fluxp);
}
    