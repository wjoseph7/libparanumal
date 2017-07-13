#include "mesh2D.h"


void acousticsPmlVolume2Dbbdg(mesh2D *mesh, iint lev){

  dfloat *cubu = (dfloat *) calloc(mesh->cubNp,sizeof(dfloat));
  dfloat *cubv = (dfloat *) calloc(mesh->cubNp,sizeof(dfloat));

  dfloat *cubpx = (dfloat *) calloc(mesh->cubNp,sizeof(dfloat));
  dfloat *cubpy = (dfloat *) calloc(mesh->cubNp,sizeof(dfloat));

  // loop over elements
  for(iint et=0;et<mesh->MRABpmlNelements[lev];++et){
    iint e = mesh->MRABpmlElementIds[lev][et];
    iint pmlId = mesh->MRABpmlIds[lev][et];

    // prefetch geometric factors (constant on triangle)
    dfloat drdx = mesh->vgeo[e*mesh->Nvgeo + RXID];
    dfloat drdy = mesh->vgeo[e*mesh->Nvgeo + RYID];
    dfloat dsdx = mesh->vgeo[e*mesh->Nvgeo + SXID];
    dfloat dsdy = mesh->vgeo[e*mesh->Nvgeo + SYID];

    for(iint n=0;n<mesh->Np;++n){     // for all nodes in this element

      // compute 'r' and 's' derivatives of (q_m) at node n
      iint D1i1 = mesh->Nfields*(e*mesh->Np + mesh->D1ids[3*n]);
      iint D2i1 = mesh->Nfields*(e*mesh->Np + mesh->D2ids[3*n]);
      iint D3i1 = mesh->Nfields*(e*mesh->Np + mesh->D3ids[3*n]);
      dfloat Dval1 = mesh->Dvals[3*n];
      
      iint D1i2 = mesh->Nfields*(e*mesh->Np + mesh->D1ids[3*n+1]);
      iint D2i2 = mesh->Nfields*(e*mesh->Np + mesh->D2ids[3*n+1]);
      iint D3i2 = mesh->Nfields*(e*mesh->Np + mesh->D3ids[3*n+1]);
      dfloat Dval2 = mesh->Dvals[3*n+1];

      iint D1i3 = mesh->Nfields*(e*mesh->Np + mesh->D1ids[3*n+2]);
      iint D2i3 = mesh->Nfields*(e*mesh->Np + mesh->D2ids[3*n+2]);
      iint D3i3 = mesh->Nfields*(e*mesh->Np + mesh->D3ids[3*n+2]);    
      dfloat Dval3 = mesh->Dvals[3*n+2];

      dfloat dudr = .5f*(Dval1*(mesh->q[D2i1+0] - mesh->q[D1i1+0]) +
                         Dval2*(mesh->q[D2i2+0] - mesh->q[D1i2+0]) +
                         Dval3*(mesh->q[D2i3+0] - mesh->q[D1i3+0]));
      dfloat duds = .5f*(Dval1*(mesh->q[D3i1+0] - mesh->q[D1i1+0]) +
                         Dval2*(mesh->q[D3i2+0] - mesh->q[D1i2+0]) +
                         Dval3*(mesh->q[D3i3+0] - mesh->q[D1i3+0]));

      dfloat dvdr = .5f*(Dval1*(mesh->q[D2i1+1] - mesh->q[D1i1+1]) +
                         Dval2*(mesh->q[D2i2+1] - mesh->q[D1i2+1]) +
                         Dval3*(mesh->q[D2i3+1] - mesh->q[D1i3+1]));
      dfloat dvds = .5f*(Dval1*(mesh->q[D3i1+1] - mesh->q[D1i1+1]) +
                         Dval2*(mesh->q[D3i2+1] - mesh->q[D1i2+1]) +
                         Dval3*(mesh->q[D3i3+1] - mesh->q[D1i3+1]));
      
      dfloat dpdr = .5f*(Dval1*(mesh->q[D2i1+2] - mesh->q[D1i1+2]) +
                         Dval2*(mesh->q[D2i2+2] - mesh->q[D1i2+2]) +
                         Dval3*(mesh->q[D2i3+2] - mesh->q[D1i3+2]));
      dfloat dpds = .5f*(Dval1*(mesh->q[D3i1+2] - mesh->q[D1i1+2]) +
                         Dval2*(mesh->q[D3i2+2] - mesh->q[D1i2+2]) +
                         Dval3*(mesh->q[D3i3+2] - mesh->q[D1i3+2]));

      // chain rule
      dfloat dudx = drdx*dudr + dsdx*duds;
      dfloat dvdy = drdy*dvdr + dsdy*dvds;
      dfloat dpdx = drdx*dpdr + dsdx*dpds;
      dfloat dpdy = drdy*dpdr + dsdy*dpds;
      
      // indices for writing the RHS terms
      iint rhsid = 3*mesh->Nfields*(e*mesh->Np + n) + mesh->Nfields*mesh->MRABshiftIndex[lev];
      iint pmlrhsid = 3*mesh->pmlNfields*(pmlId*mesh->Np + n) + mesh->pmlNfields*mesh->MRABshiftIndex[lev];

      // store acoustics rhs contributions from collocation differentiation
      mesh->rhsq[rhsid+0] = -dpdx;
      mesh->rhsq[rhsid+1] = -dpdy;
      
      mesh->pmlrhsq[pmlrhsid+0] = -dudx;
      mesh->pmlrhsq[pmlrhsid+1] = -dvdy;
    }

    // Interpolate to cubature nodes
    for(iint n=0;n<mesh->cubNp;++n){
      dfloat u = 0.f;
      dfloat v = 0.f;
      dfloat px = 0.f;
      dfloat py = 0.f;
      for (iint i=0;i<mesh->Np;++i){
        iint base = mesh->Nfields*(e*mesh->Np + i);
        u += mesh->cubInterp[n*mesh->Np + i] * mesh->q[base+0];
        v += mesh->cubInterp[n*mesh->Np + i] * mesh->q[base+1];

        iint pmlBase = mesh->pmlNfields*(pmlId*mesh->Np+i);
        px += mesh->cubInterp[n*mesh->Np + i] * mesh->pmlq[pmlBase+0];
        py += mesh->cubInterp[n*mesh->Np + i] * mesh->pmlq[pmlBase+1];
      }    

      dfloat sigmax = mesh->pmlSigmaX[pmlId*mesh->cubNp + n];
      dfloat sigmay = mesh->pmlSigmaY[pmlId*mesh->cubNp + n];

      cubu[n] = -sigmax*u;
      cubv[n] = -sigmay*v;
      cubpx[n] = -sigmax*px;
      cubpy[n] = -sigmay*py;
    }

    //Project down and store
    for(iint n=0;n<mesh->Np;++n){
      iint rhsid = 3*mesh->Nfields*(e*mesh->Np + n) + mesh->Nfields*mesh->MRABshiftIndex[lev];
      iint pmlrhsid = 3*mesh->pmlNfields*(pmlId*mesh->Np + n) + mesh->pmlNfields*mesh->MRABshiftIndex[lev];

      dfloat rhsu = mesh->rhsq[rhsid+0];
      dfloat rhsv = mesh->rhsq[rhsid+1];
      dfloat rhspx = mesh->pmlrhsq[pmlrhsid+0];
      dfloat rhspy = mesh->pmlrhsq[pmlrhsid+1];
      for (iint i=0;i<mesh->cubNp;++i){
        rhsu += mesh->cubProject[n*mesh->cubNp + i] * cubu[i];
        rhsv += mesh->cubProject[n*mesh->cubNp + i] * cubv[i];
        rhspx += mesh->cubProject[n*mesh->cubNp + i] * cubpx[i];
        rhspy += mesh->cubProject[n*mesh->cubNp + i] * cubpy[i];
      }

      mesh->rhsq[rhsid+0] = rhsu;
      mesh->rhsq[rhsid+1] = rhsv;
      mesh->pmlrhsq[pmlrhsid+0] = rhspx;
      mesh->pmlrhsq[pmlrhsid+1] = rhspy;
    }
  }

  free(cubu); free(cubv);
  free(cubpx); free(cubpy);
}