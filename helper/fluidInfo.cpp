/*
This file is part of the HemoCell library

HemoCell is developed and maintained by the Computational Science Lab 
in the University of Amsterdam. Any questions or remarks regarding this library 
can be sent to: info@hemocell.eu

When using the HemoCell library in scientific work please cite the
corresponding paper: https://doi.org/10.3389/fphys.2017.00563

The HemoCell library is free software: you can redistribute it and/or
modify it under the terms of the GNU Affero General Public License as
published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

The library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "fluidInfo.h"
#include "hemoCellParticleField.h"
#include "hemocell.h"

void GatherFluidVelocity::processGenericBlocks(Box3D domain, std::vector<AtomicBlock3D*> blocks) {
    BlockLattice3D<double,DESCRIPTOR>* ff = dynamic_cast<BlockLattice3D<double,DESCRIPTOR>*>(blocks[0]);
    HEMOCELL_PARTICLE_FIELD* pf = dynamic_cast<HEMOCELL_PARTICLE_FIELD*>(blocks[1]);
    plb::Array<double,3> vel_vec;
    ff->grid[domain.x0][domain.y0][domain.z0].computeVelocity(vel_vec);
    double vel = sqrt(vel_vec[0]*vel_vec[0]+vel_vec[1]*vel_vec[1]+vel_vec[2]*vel_vec[2]);
    double min=vel,max=vel,avg=0.;
    
    
    for (plint iX=domain.x0; iX<=domain.x1; ++iX) {
      for (plint iY=domain.y0; iY<=domain.y1; ++iY) {
        for (plint iZ=domain.z0; iZ<=domain.z1; ++iZ) {
          ff->grid[iX][iY][iZ].computeVelocity(vel_vec);
          vel = sqrt(vel_vec[0]*vel_vec[0]+vel_vec[1]*vel_vec[1]+vel_vec[2]*vel_vec[2]);
          min = min > vel ? vel : min;
          max = max < vel ? vel : max;
          avg += vel;
        }
      }
    }
    
    gatherValues[pf->atomicBlockId].min = min;
    gatherValues[pf->atomicBlockId].max = max;
    gatherValues[pf->atomicBlockId].avg = avg/domain.nCells();
    gatherValues[pf->atomicBlockId].ncells = domain.nCells();
}
void GatherFluidForce::processGenericBlocks(Box3D domain, std::vector<AtomicBlock3D*> blocks) {
    BlockLattice3D<double,DESCRIPTOR>* ff = dynamic_cast<BlockLattice3D<double,DESCRIPTOR>*>(blocks[0]);
    HEMOCELL_PARTICLE_FIELD* pf = dynamic_cast<HEMOCELL_PARTICLE_FIELD*>(blocks[1]);
    hemo::Array<double,3> vel_vec;
    vel_vec[0] = ff->grid[domain.x0][domain.y0][domain.z0].external.data[0];
    vel_vec[1] = ff->grid[domain.x0][domain.y0][domain.z0].external.data[1];
    vel_vec[2] = ff->grid[domain.x0][domain.y0][domain.z0].external.data[2];

    double vel = sqrt(vel_vec[0]*vel_vec[0]+vel_vec[1]*vel_vec[1]+vel_vec[2]*vel_vec[2]);
    double min=vel,max=vel,avg=0.;
    
    
    for (plint iX=domain.x0; iX<=domain.x1; ++iX) {
      for (plint iY=domain.y0; iY<=domain.y1; ++iY) {
        for (plint iZ=domain.z0; iZ<=domain.z1; ++iZ) {
          vel_vec[0] = ff->grid[iX][iY][iZ].external.data[0];
          vel_vec[1] = ff->grid[iX][iY][iZ].external.data[1];
          vel_vec[2] = ff->grid[iX][iY][iZ].external.data[2];
          
          vel = sqrt(vel_vec[0]*vel_vec[0]+vel_vec[1]*vel_vec[1]+vel_vec[2]*vel_vec[2]);
          min = min > vel ? vel : min;
          max = max < vel ? vel : max;
          avg += vel;
        }
      }
    }
    
    gatherValues[pf->atomicBlockId].min = min;
    gatherValues[pf->atomicBlockId].max = max;
    gatherValues[pf->atomicBlockId].avg = avg/domain.nCells();
    gatherValues[pf->atomicBlockId].ncells = domain.nCells();
}

FluidStatistics FluidInfo::calculateVelocityStatistics(HemoCell* hemocell) {
  map<int,FluidStatistics> gatherValues;

  vector<MultiBlock3D*> wrapper;
  wrapper.push_back(hemocell->cellfields->lattice);
  wrapper.push_back(hemocell->cellfields->immersedParticles);
  applyTimedProcessingFunctional(new GatherFluidVelocity(gatherValues),hemocell->cellfields->immersedParticles->getBoundingBox(),wrapper);
  int numAtomicBlock = hemocell->lattice->getMultiBlockManagement().getSparseBlockStructure().getNumBlocks();
  HemoCellGatheringFunctional<FluidStatistics>::gather(gatherValues,numAtomicBlock);
  
  FluidStatistics result = gatherValues.begin()->second;
  result.avg = 0.;
  result.ncells = 0;
  
  for (const auto & pair : gatherValues) {
    const FluidStatistics & cur = pair.second;
    result.avg += (cur.avg * cur.ncells);
    result.ncells += cur.ncells;
    result.min = result.min > cur.min ? cur.min : result.min;
    result.max = result.max < cur.max ? cur.max : result.max;
  }
  
  result.avg /= result.ncells;

  return result;
}
FluidStatistics FluidInfo::calculateForceStatistics(HemoCell* hemocell) {
  pcout << "(FLuidInfo) (CalculateForceStatistics) Warning! You must reapply any external force after calling this function!" << endl;
  hemocell->cellfields->spreadParticleForce();  
  
  map<int,FluidStatistics> gatherValues;
  
  vector<MultiBlock3D*> wrapper;
  wrapper.push_back(hemocell->cellfields->lattice);
  wrapper.push_back(hemocell->cellfields->immersedParticles);
  applyTimedProcessingFunctional(new GatherFluidForce(gatherValues),hemocell->cellfields->immersedParticles->getBoundingBox(),wrapper);
  int numAtomicBlock = hemocell->lattice->getMultiBlockManagement().getSparseBlockStructure().getNumBlocks();
  HemoCellGatheringFunctional<FluidStatistics>::gather(gatherValues,numAtomicBlock);
  
  FluidStatistics result = gatherValues.begin()->second;
  result.avg = 0.;
  result.ncells = 0;
  
  for (const auto & pair : gatherValues) {
    const FluidStatistics & cur = pair.second;
    result.avg += (cur.avg * cur.ncells);
    result.ncells += cur.ncells;
    result.min = result.min > cur.min ? cur.min : result.min;
    result.max = result.max < cur.max ? cur.max : result.max;
  }
  
  result.avg /= result.ncells;

  setExternalVector(*hemocell->lattice, (*hemocell->lattice).getBoundingBox(),
          DESCRIPTOR<T>::ExternalField::forceBeginsAt,
          plb::Array<T, DESCRIPTOR<T>::d>(0.0, 0.0, 0.0));
  
  return result;
}

GatherFluidVelocity * GatherFluidVelocity::clone() const { return new GatherFluidVelocity(*this); }
GatherFluidForce * GatherFluidForce::clone() const { return new GatherFluidForce(*this); }
