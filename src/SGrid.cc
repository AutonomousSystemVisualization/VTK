/*=========================================================================

  Program:   Visualization Library
  Module:    SGrid.cc
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

Description:
---------------------------------------------------------------------------
This file is part of the Visualization Library. No part of this file
or its contents may be copied, reproduced or altered in any way
without the express written consent of the authors.

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen 1993, 1994 

=========================================================================*/
#include "SGrid.hh"
#include "Point.hh"
#include "Line.hh"
#include "Quad.hh"
#include "Hexa.hh"

vlStructuredGrid::vlStructuredGrid()
{
}

vlStructuredGrid::~vlStructuredGrid()
{
  this->Initialize();
}

void vlStructuredGrid::Initialize()
{
  vlPointSet::Initialize(); 
  vlStructuredDataSet::Initialize();
}

vlCell *vlStructuredGrid::GetCell(int cellId)
{
  static vlPoint point;
  static vlLine line;
  static vlQuad quad;
  static vlHexahedron hexa;
  static vlCell *cell;
  int i, j, k, idx, loc[3], npts;
  int iMin, iMax, jMin, jMax, kMin, kMax;
  int d01 = this->Dimensions[0]*this->Dimensions[1];
  float *x;
 
  // Make sure data is defined
  if ( ! this->Points )
    {
    vlErrorMacro (<<"No data");
    return 0;
    }

  // 
  switch (this->DataDescription)
    {
    case SINGLE_POINT: // cellId can only be = 0
      iMin = iMax = jMin = jMax = kMin = kMax = 0;
      cell = &point;
      break;

    case X_LINE:
      jMin = jMax = kMin = kMax = 0;
      iMin = cellId;
      iMax = cellId + 1;
      cell = &line;
      break;

    case Y_LINE:
      iMin = iMax = kMin = kMax = 0;
      jMin = cellId;
      jMax = cellId + 1;
      cell = &line;
      break;

    case Z_LINE:
      iMin = iMax = jMin = jMax = 0;
      kMin = cellId;
      kMax = cellId + 1;
      cell = &line;
      break;

    case XY_PLANE:
      kMin = kMax = 0;
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      jMin = cellId / (this->Dimensions[0]-1);
      jMax = jMin + 1;
      cell = &quad;
      break;

    case YZ_PLANE:
      iMin = iMax = 0;
      jMin = cellId % (this->Dimensions[1]-1);
      jMax = jMin + 1;
      kMin = cellId / (this->Dimensions[1]-1);
      kMax = kMin + 1;
      cell = &quad;
      break;

    case XZ_PLANE:
      jMin = jMax = 0;
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      kMin = cellId / (this->Dimensions[0]-1);
      kMax = kMin + 1;
      cell = &quad;
      break;

    case XYZ_GRID:
      { // braces necessary because some compilers don't like jumping
        // over initializers
      int cd01 = this->Dimensions[0]*this->Dimensions[1];
      iMin = cellId % (this->Dimensions[0]-1);
      iMax = iMin + 1;
      jMin = (cellId % cd01) / (this->Dimensions[0]-1);
      jMax = jMin + 1;
      kMin = cellId / cd01;
      kMax = kMin + 1;
      cell = &hexa;
      }
      break;
    }

  // Extract point coordinates and point ids
  for (npts=0,loc[2]=kMin; loc[2]<=kMax; loc[2]++)
    {
    for (loc[1]=jMin; loc[1]<=jMax; loc[1]++)
      {
      for (loc[0]=iMin; loc[0]<=iMax; loc[0]++)
        {
        idx = loc[0] + loc[1]*this->Dimensions[0] + loc[2]*d01;
        x = this->Points->GetPoint(idx);
        cell->PointIds.InsertId(npts,idx);
        cell->Points.InsertPoint(npts++,x);
        }
      }
    }

  return cell;
}

void vlStructuredGrid::PrintSelf(ostream& os, vlIndent indent)
{
  if (this->ShouldIPrint(vlStructuredGrid::GetClassName()))
    {
    vlDataSet::PrintSelf(os,indent);
    }
}

