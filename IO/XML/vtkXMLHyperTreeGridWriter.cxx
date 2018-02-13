/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkXMLHyperTreeGridWriter.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

//TODO:
// Add support for timesteps
// Add streaming support.

#include "vtkXMLHyperTreeGridWriter.h"

#include "vtkBitArray.h"
#include "vtkDoubleArray.h"
#include "vtkErrorCode.h"
#include "vtkHyperTreeGrid.h"
#include "vtkHyperTreeGridCursor.h"
#include "vtkIdTypeArray.h"
#include "vtkInformation.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"

vtkStandardNewMacro(vtkXMLHyperTreeGridWriter);

//----------------------------------------------------------------------------
vtkXMLHyperTreeGridWriter::vtkXMLHyperTreeGridWriter()
{
}

//----------------------------------------------------------------------------
vtkXMLHyperTreeGridWriter::~vtkXMLHyperTreeGridWriter()
{
}

//----------------------------------------------------------------------------
void vtkXMLHyperTreeGridWriter::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}

//----------------------------------------------------------------------------
vtkHyperTreeGrid* vtkXMLHyperTreeGridWriter::GetInput()
{
  return static_cast<vtkHyperTreeGrid*>(this->Superclass::GetInput());
}

//----------------------------------------------------------------------------
const char* vtkXMLHyperTreeGridWriter::GetDefaultFileExtension()
{
  return "vhg";
}

//----------------------------------------------------------------------------
const char* vtkXMLHyperTreeGridWriter::GetDataSetName()
{
  return "HyperTreeGrid";
}

//----------------------------------------------------------------------------
int vtkXMLHyperTreeGridWriter::FillInputPortInformation(
  int, vtkInformation* info)
{
  info->Set(vtkAlgorithm::INPUT_REQUIRED_DATA_TYPE(), "vtkHyperTreeGrid");
  return 1;
}

//----------------------------------------------------------------------------
int vtkXMLHyperTreeGridWriter::WriteData()
{
  //write XML header and VTK file header and file attributes
  if (!this->StartFile())
  {
    return 0;
  }

  vtkIndent indent = vtkIndent().GetNextIndent();

  // Header attributes
  if (!this->StartPrimaryElement(indent))
  {
    return 0;
  }

  // Coordinates for grid (can be replaced by origin and scale)
  this->WriteGridCoordinates(indent.GetNextIndent());

  if (!this->WriteDescriptor(indent.GetNextIndent()))
  {
    return 0;
  }

  if (!this->WriteAttributeData(indent.GetNextIndent()))
  {
    return 0;
  }

  this->WriteFieldData(indent.GetNextIndent());

  if (!this->FinishPrimaryElement(indent))
  {
    return 0;
  }

  if (!this->EndFile())
  {
    return 0;
  }
  return 1;
}

//----------------------------------------------------------------------------
int vtkXMLHyperTreeGridWriter::StartPrimaryElement(vtkIndent indent)
{
  ostream& os = *(this->Stream);

  return (!this->WritePrimaryElement(os, indent)) ? 0 : 1;
}

//----------------------------------------------------------------------------
void vtkXMLHyperTreeGridWriter::WritePrimaryElementAttributes
  (ostream &os, vtkIndent indent)
{
  this->Superclass::WritePrimaryElementAttributes(os, indent);
  vtkHyperTreeGrid* input = this->GetInput();

  this->WriteScalarAttribute("Dimension", (int) input->GetDimension());
  this->WriteScalarAttribute("BranchFactor", (int) input->GetBranchFactor());
  this->WriteScalarAttribute("TransposedRootIndexing",
                             (bool)input->GetTransposedRootIndexing());
  this->WriteVectorAttribute("GridSize", 3, (int*) input->GetGridSize());

  // vtkHyperTreeGrid does not yet store origin and scale but
  // calculate as place holder
  vtkDoubleArray* xcoord =
    vtkDoubleArray::SafeDownCast(input->GetXCoordinates());
  vtkDoubleArray* ycoord =
    vtkDoubleArray::SafeDownCast(input->GetYCoordinates());
  vtkDoubleArray* zcoord =
    vtkDoubleArray::SafeDownCast(input->GetZCoordinates());

  double gridOrigin[3] = {xcoord->GetValue(0),
                          ycoord->GetValue(0),
                          zcoord->GetValue(0)};

  double gridScale[3] = {xcoord->GetValue(1) - xcoord->GetValue(0),
                         ycoord->GetValue(1) - ycoord->GetValue(0),
                         zcoord->GetValue(1) - zcoord->GetValue(0)};

  this->WriteVectorAttribute("GridOrigin", 3, gridOrigin);
  this->WriteVectorAttribute("GridScale", 3, gridScale);
}

//----------------------------------------------------------------------------
void vtkXMLHyperTreeGridWriter::WriteGridCoordinates(vtkIndent indent)
{
  vtkHyperTreeGrid* input = this->GetInput();
  ostream& os = *(this->Stream);
  os << indent << "<Coordinates>\n";
  os.flush();

  this->WriteArrayInline(input->GetXCoordinates(), indent.GetNextIndent(),
                         "XCoordinates",
                         input->GetXCoordinates()->GetNumberOfValues());
  this->WriteArrayInline(input->GetYCoordinates(), indent.GetNextIndent(),
                         "YCoordinates",
                         input->GetYCoordinates()->GetNumberOfValues());
  this->WriteArrayInline(input->GetZCoordinates(), indent.GetNextIndent(),
                         "ZCoordinates",
                         input->GetZCoordinates()->GetNumberOfValues());

  os << indent << "</Coordinates>\n";
  os.flush();
}

//----------------------------------------------------------------------------
int vtkXMLHyperTreeGridWriter::WriteDescriptor(vtkIndent indent)
{
  vtkHyperTreeGrid* input = this->GetInput();
  int numberOfTrees = input->GetNumberOfTrees();
  vtkIdType maxLevels = input->GetNumberOfLevels();

  ostream& os = *(this->Stream);
  os << indent << "<Topology>\n";
  os.flush();

  // All trees contained on this processor
  vtkIdTypeArray* treeIds = input->GetMaterialMaskIndex();
  if (treeIds)
  {
    this->WriteArrayInline(treeIds, indent.GetNextIndent(),
                           "MaterialMaskIndex", numberOfTrees);
  }

  // Collect description by processing depth first and writing breadth first
  std::string *descByLevel = new std::string[maxLevels];
  vtkIdType inIndex;
  vtkHyperTreeGrid::vtkHyperTreeGridIterator it;
  input->InitializeTreeIterator( it );
  while ( it.GetNextTree( inIndex ) )
  {
    // Initialize new grid cursor at root of current input tree
    vtkHyperTreeGridCursor* inCursor = input->NewGridCursor( inIndex );
    // Recursively compute descriptor for this tree, appending any
    // entries for each of the levels in descByLevel.
    this->BuildDescriptor(inCursor, 0, descByLevel );
    // Clean up
    inCursor->Delete();
  }

  // Build the BitArray from the level descriptors
  vtkNew<vtkBitArray> descriptor;

  std::string::const_iterator dit;
  for (int l = 0; l < maxLevels; l++)
  {
    for (dit = descByLevel[l].begin(); dit != descByLevel[l].end(); ++dit)
    {
      switch (*dit)
      {
        case 'R':    //  Refined cell
          descriptor->InsertNextValue(1);
          break;

        case '.':    // Leaf cell
          descriptor->InsertNextValue(0);
          break;

        default:
          vtkErrorMacro(<< "Unrecognized character: "
                        << *dit
                        << " in string "
                        << descByLevel[l]);
          return 0;
      }
    }
  }
  descriptor->Squeeze();
  this->WriteArrayInline(descriptor, indent.GetNextIndent(), "Descriptor",
                         descriptor->GetNumberOfValues());

  os << indent << "</Topology>\n";
  os.flush();

  delete[] descByLevel;
  return 1;
}

//----------------------------------------------------------------------------
void vtkXMLHyperTreeGridWriter::BuildDescriptor
( vtkHyperTreeGridCursor* inCursor,
  int level,
  std::string* descriptor)
{
  // Retrieve input grid
  vtkHyperTreeGrid* input = inCursor->GetGrid();

  if ( ! inCursor->IsLeaf() )
  {
    descriptor[level] += 'R';

    // If input cursor is not a leaf, recurse to all children
    int numChildren = input->GetNumberOfChildren();
    for ( int child = 0; child < numChildren; ++ child )
    {
      // Create child cursor from parent in input grid
      vtkHyperTreeGridCursor* childCursor = inCursor->Clone();
      childCursor->ToChild( child );

      // Recurse
      this->BuildDescriptor( childCursor, level+1, descriptor );

      // Clean up
      childCursor->Delete();
    } // child
  }
  else
  {
    descriptor[level] += '.';
  }
}

//----------------------------------------------------------------------------
int vtkXMLHyperTreeGridWriter::WriteAttributeData(vtkIndent indent)
{
  // Write the point data and cell data arrays.
  vtkDataSet* input = this->GetInputAsDataSet();

  // Split progress between point data and cell data arrays.
  float progressRange[2] = { 0.f, 0.f };
  this->GetProgressRange(progressRange);
  int pdArrays = input->GetPointData()->GetNumberOfArrays();
  int total = (pdArrays)? (pdArrays):1;
  float fractions[3] = { 0.f, static_cast<float>(pdArrays) / total, 1.f };

  // Set the range of progress for the point data arrays.
  this->SetProgressRange(progressRange, 0, fractions);

  this->WritePointDataInline(input->GetPointData(), indent);

  return 1;
}

//----------------------------------------------------------------------------
int vtkXMLHyperTreeGridWriter::FinishPrimaryElement(vtkIndent indent)
{
  ostream& os = *(this->Stream);

  // End the primary element.
  os << indent << "</" << this->GetDataSetName() << ">\n";
  os.flush();
  if (os.fail())
  {
    this->SetErrorCode(vtkErrorCode::OutOfDiskSpaceError);
    return 0;
  }
  return 1;
}
