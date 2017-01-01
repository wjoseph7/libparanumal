#include "boltzmann2D.h"

void boltzmannSplitPmlRun2D(mesh2D *mesh){

  // MPI send buffer
  iint haloBytes = mesh->totalHaloPairs*mesh->Np*mesh->Nfields*sizeof(dfloat);
  dfloat *sendBuffer = (dfloat*) malloc(haloBytes);
  dfloat *recvBuffer = (dfloat*) malloc(haloBytes);
  
  // Low storage explicit Runge Kutta (5 stages, 4th order)
  for(iint tstep=0;tstep<mesh->NtimeSteps;++tstep){

    for(iint rk=0;rk<mesh->Nrk;++rk){
      // intermediate stage time
      dfloat t = tstep*mesh->dt + mesh->dt*mesh->rkc[rk];

      //      mesh->o_q.copyTo(mesh->q);
      //      boltzmannError2D(mesh, mesh->dt*(tstep+1));

      if(mesh->totalHaloPairs>0){
	// extract halo on DEVICE
	iint Nentries = mesh->Np*mesh->Nfields;
	
	mesh->haloExtractKernel(mesh->totalHaloPairs,
				Nentries,
				mesh->o_haloElementList,
				mesh->o_q,
				mesh->o_haloBuffer);
	
	// copy extracted halo to HOST 
	mesh->o_haloBuffer.copyTo(sendBuffer);      
	
	// start halo exchange
	meshHaloExchangeStart2D(mesh,
				mesh->Np*mesh->Nfields*sizeof(dfloat),
				sendBuffer,
				recvBuffer);
      }

      // compute volume contribution to DG boltzmann RHS
      mesh->volumeKernel(mesh->Nelements,
			 mesh->o_vgeo,
			 mesh->o_sigmax,
			 mesh->o_sigmay,
			 mesh->o_DrT,
			 mesh->o_DsT,
			 mesh->o_q,
			 mesh->o_pmlqx,
			 mesh->o_pmlqy,
			 mesh->o_pmlNT,
			 mesh->o_rhspmlqx,
			 mesh->o_rhspmlqy,
			 mesh->o_rhspmlNT);


      // compute relaxation terms using cubature
      mesh->relaxationKernel(mesh->Nelements,
			     mesh->o_cubInterpT,
			     mesh->o_cubProjectT,
			     mesh->o_q,
			     mesh->o_rhspmlqx,
			     mesh->o_rhspmlqy,
			     mesh->o_rhspmlNT);

      if(mesh->totalHaloPairs>0){
	// wait for halo data to arrive
	meshHaloExchangeFinish2D(mesh);
	
	// copy halo data to DEVICE
	size_t offset = mesh->Np*mesh->Nfields*mesh->Nelements*sizeof(dfloat); // offset for halo data
	mesh->o_q.copyFrom(recvBuffer, haloBytes, offset);
      }

      // compute surface contribution to DG boltzmann RHS
      dfloat ramp = boltzmannRampFunction2D(t);

      mesh->surfaceKernel(mesh->Nelements,
			  mesh->o_sgeo,
			  mesh->o_LIFTT,
			  mesh->o_vmapM,
			  mesh->o_vmapP,
			  mesh->o_EToB,
			  t,
			  mesh->o_x,
			  mesh->o_y,
			  ramp,
			  mesh->o_q,
			  mesh->o_rhspmlqx,
			  mesh->o_rhspmlqy);
      
      // update solution using Runge-Kutta
      iint recombine = 0; // do not recombine split fields

      // ramp function for flow at next RK stage
      dfloat tupdate = tstep*mesh->dt + mesh->dt*mesh->rkc[rk+1];
      dfloat rampUpdate = boltzmannRampFunction2D(tupdate);

      mesh->updateKernel(mesh->Nelements,
			 recombine,
			 mesh->dt,
			 mesh->rka[rk],
			 mesh->rkb[rk],
			 rampUpdate,
			 mesh->o_rhspmlqx,
			 mesh->o_rhspmlqy,
			 mesh->o_rhspmlNT,
			 mesh->o_respmlqx,
			 mesh->o_respmlqy,
			 mesh->o_respmlNT,
			 mesh->o_pmlqx,
			 mesh->o_pmlqy,
			 mesh->o_pmlNT,
			 mesh->o_q);
      
    }
    
    // estimate maximum error
    if((tstep%mesh->errorStep)==0){
      dfloat t = (tstep+1)*mesh->dt;

      // report ramp function
      int rank;
      MPI_Comm_rank(MPI_COMM_WORLD, &rank);
      if(rank==0){
	dfloat ramp = boltzmannRampFunction2D(t);
	printf("t: %g ramp: %g\n", t, ramp);
      }
      
      // copy data back to host
      mesh->o_q.copyTo(mesh->q);
      
      // do error stuff on host
      boltzmannError2D(mesh, t);

      // compute vorticity
      boltzmannComputeVorticity2D(mesh, mesh->q, 0, mesh->Nfields);
      
      // output field files
      iint fld = 0;
      char fname[BUFSIZ];
      sprintf(fname, "foo_T%04d", tstep/mesh->errorStep);
      meshPlotVTU2D(mesh, fname, fld);
    }
  }

  free(recvBuffer);
  free(sendBuffer);
}



