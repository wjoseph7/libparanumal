#include "boltzmannQuad3D.h"

int main(int argc, char **argv){

  // start up MPI
  MPI_Init(&argc, &argv);

  if(argc!=3){
    // to run cavity test case with degree N elements
    printf("usage: ./main meshes/cavityH005.msh N\n");
    exit(-1);
  }

  // int specify polynomial degree 
  int N = atoi(argv[2]);

  // set up mesh stuff
  mesh_t *mesh = meshSetupQuad3D(argv[1], N);

  // set up boltzmann stuff
  solver_t *solver = boltzmannSetupQuad3D(mesh);

  // time step Boltzmann equations
  boltzmannRunQuad3D(solver);

  // close down MPI
  MPI_Finalize();

  exit(0);
  return 0;
}