/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkSynchronizedTemplates2D.cxx
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 1993-2002 Ken Martin, Will Schroeder, Bill Lorensen 
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even 
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR 
     PURPOSE.  See the above copyright notice for more information.

     THIS CLASS IS PATENT PENDING.

     Application of this software for commercial purposes requires 
     a license grant from Kitware. Contact:
         Ken Martin
         Kitware
         469 Clifton Corporate Parkway,
         Clifton Park, NY 12065
         Phone:1-518-371-3971 
     for more information.

=========================================================================*/
#include "vtkSynchronizedTemplates2D.h"

#include "vtkCellArray.h"
#include "vtkCellData.h"
#include "vtkCharArray.h"
#include "vtkDoubleArray.h"
#include "vtkFloatArray.h"
#include "vtkImageData.h"
#include "vtkIntArray.h"
#include "vtkLongArray.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkPolyData.h"
#include "vtkShortArray.h"
#include "vtkUnsignedCharArray.h"
#include "vtkUnsignedIntArray.h"
#include "vtkUnsignedLongArray.h"
#include "vtkUnsignedShortArray.h"

#include <math.h>

vtkCxxRevisionMacro(vtkSynchronizedTemplates2D, "1.31");
vtkStandardNewMacro(vtkSynchronizedTemplates2D);

//----------------------------------------------------------------------------
// Description:
// Construct object with initial scalar range (0,1) and single contour value
// of 0.0. The ImageRange are set to extract the first k-plane.
vtkSynchronizedTemplates2D::vtkSynchronizedTemplates2D()
{
  this->ContourValues = vtkContourValues::New();
  this->ComputeScalars = 1;
  this->InputScalarsSelection = NULL;
  this->ArrayComponent = 0;
}

vtkSynchronizedTemplates2D::~vtkSynchronizedTemplates2D()
{
  this->ContourValues->Delete();
  this->SetInputScalarsSelection(NULL);
}



//----------------------------------------------------------------------------
// Specify the input data or filter.
void vtkSynchronizedTemplates2D::SetInput(vtkImageData *input)
{
  this->vtkProcessObject::SetNthInput(0, input);
}

//----------------------------------------------------------------------------
// Specify the input data or filter.
vtkImageData *vtkSynchronizedTemplates2D::GetInput()
{
  if (this->NumberOfInputs < 1)
    {
    return NULL;
    }
  
  return (vtkImageData *)(this->Inputs[0]);
}

//----------------------------------------------------------------------------
// Description:
// Overload standard modified time function. If contour values are modified,
// then this object is modified as well.
unsigned long vtkSynchronizedTemplates2D::GetMTime()
{
  unsigned long mTime=this->vtkPolyDataSource::GetMTime();
  unsigned long mTime2=this->ContourValues->GetMTime();

  mTime = ( mTime2 > mTime ? mTime2 : mTime );
  return mTime;
}


//----------------------------------------------------------------------------
//
// Contouring filter specialized for images
//
template <class T>
void vtkContourImage(vtkSynchronizedTemplates2D *self,
                     T *scalars, vtkPoints *newPts,
                     vtkDataArray *newScalars, vtkCellArray *lines)
{
  float *values = self->GetValues();
  int numContours = self->GetNumberOfContours();
  T *inPtr, *rowPtr;
  float x[3];
  float *origin = self->GetInput()->GetOrigin();
  float *spacing = self->GetInput()->GetSpacing();
  float y, t;
  int *isect1Ptr, *isect2Ptr;
  vtkIdType ptIds[2];
  int *tablePtr;
  int v0, v1 = 0, v2;
  int idx, vidx;
  float s0, s1, s2, value;
  int i, j;
  int lineCases[64];
  
  // The update extent may be different than the extent of the image.
  // The only problem with using the update extent is that one or two 
  // sources enlarge the update extent.  This behavior is slated to be 
  // eliminated.
  int *incs = self->GetInput()->GetIncrements();
  int *ext = self->GetInput()->GetExtent();
  int *updateExt = self->GetInput()->GetUpdateExtent();
  int axis0, axis1;
  int min0, max0, dim0;
  int min1, max1;
  int inc0, inc1;
  
  // Figure out which plane the image lies in.
  if (updateExt[4] == updateExt[5])
    { // z collapsed
    axis0 = 0;
    min0 = updateExt[0];
    max0 = updateExt[1];
    inc0 = incs[0];
    axis1 = 1;
    min1 = updateExt[2];
    max1 = updateExt[3];
    inc1 = incs[1];
    x[2] = origin[2] + (updateExt[4]*spacing[2]);
    }
  else if (updateExt[2] == updateExt[3])
    { // y collapsed
    axis0 = 0;
    min0 = updateExt[0];
    max0 = updateExt[1];
    inc0 = incs[0];
    axis1 = 2;
    min1 = updateExt[4];
    max1 = updateExt[5];
    inc1 = incs[2];
    x[1] = origin[1] + (updateExt[2]*spacing[1]);
    }
  else if (updateExt[0] == updateExt[1])
    { // x collapsed
    axis0 = 1;
    min0 = updateExt[2];
    max0 = updateExt[3];
    inc0 = incs[1];
    axis1 = 2;
    min1 = updateExt[4];
    max1 = updateExt[5];
    inc1 = incs[2];
    x[0] = origin[0] + (updateExt[0]*spacing[0]);
    }
  else 
    {
    vtkGenericWarningMacro("Expecting 2D data.");
    return;
    }
  dim0 = max0-min0+1;
  
  
  // setup the table entries
  for (i = 0; i < 64; i++)
    {
    lineCases[i] = -1;
    }
  
  lineCases[12] = 3;
  lineCases[13] = dim0*2;

  lineCases[20] = 1;
  lineCases[21] = dim0*2;

  lineCases[24] = 1;
  lineCases[25] = 3;

  lineCases[36] = 0;
  lineCases[37] = dim0*2;

  lineCases[40] = 0;
  lineCases[41] = 3;

  lineCases[48] = 0;
  lineCases[49] = 1;

  lineCases[60] = 0;
  lineCases[61] = 1;
  lineCases[62] = 3;
  lineCases[63] = dim0*2;
    
  // allocate storage arrays
  int *isect1 = new int [dim0*4];
  isect1[dim0*2-2] = -1;
  isect1[dim0*2-1] = -1;
  isect1[dim0*4-2] = -1;
  isect1[dim0*4-1] = -1;

  
  // Compute the staring location.  We may be operating
  // on a part of the image.
  scalars += incs[0]*(updateExt[0]-ext[0]) 
    + incs[1]*(updateExt[2]-ext[2])
    + incs[2]*(updateExt[4]-ext[4]) + self->GetArrayComponent();
  
  // for each contour
  for (vidx = 0; vidx < numContours; vidx++)
    {
    rowPtr = scalars;
    inPtr = rowPtr;

    lineCases[13] = dim0*2;
    lineCases[21] = dim0*2;
    lineCases[37] = dim0*2;
    lineCases[63] = dim0*2;
    
    value = values[vidx];
    
    // Traverse all pixel cells, generating line segements using templates
    for (j = min1; j <= max1; j++)
      {
      inPtr = rowPtr;
      rowPtr += inc1;
      
      // set the y coordinate
      y = origin[axis1] + j*spacing[axis1];
      // first compute the intersections
      s1 = *inPtr;
      
      // swap the buffers
      if (j%2)
        {
        lineCases[13] = dim0*2;
        lineCases[21] = dim0*2;
        lineCases[37] = dim0*2;
        lineCases[63] = dim0*2;
        isect1Ptr = isect1;
        isect2Ptr = isect1 + dim0*2;
        }
      else
        {
        lineCases[13] = -dim0*2;
        lineCases[21] = -dim0*2;
        lineCases[37] = -dim0*2;
        lineCases[63] = -dim0*2;
        isect1Ptr = isect1 + dim0*2;
        isect2Ptr = isect1;
        }
      
      for (i = min0; i < max0; i++)
        {
        s0 = s1;
        s1 = *(inPtr + inc0);
        // compute in/out for verts
        v0 = (s0 < value ? 0 : 1);
        v1 = (s1 < value ? 0 : 1);
        if (v0 ^ v1)
          {
          t = (value - s0) / (s1 - s0);
          x[axis0] = origin[axis0] + spacing[axis0]*(i+t);
          x[axis1] = y;
          *isect2Ptr = newPts->InsertNextPoint(x);
          if (newScalars)
            {
            newScalars->InsertNextTuple(&value);
            }
          }
        else
          {
          *isect2Ptr = -1;
          }
        if (j < max1)
          {
          s2 = *(inPtr + inc1);
          v2 = (s2 < value ? 0 : 1);
          if (v0 ^ v2)
            {
            t = (value - s0) / (s2 - s0);
            x[axis0] = origin[axis0] + spacing[axis0]*i;
            x[axis1] = y + spacing[axis1]*t;
            *(isect2Ptr + 1) = newPts->InsertNextPoint(x);
            if (newScalars)
              {
              newScalars->InsertNextTuple(&value);
              }
            }
          else
            {
            *(isect2Ptr + 1) = -1;
            }
          }
        
        if (j > min1)
          {       
          // now add any lines that need to be added
          // basically look at the isect values, 
          // form an index and lookup the lines
          idx = (*isect1Ptr > -1 ? 8 : 0);
          idx = idx + (*(isect1Ptr +1) > -1 ? 4 : 0);
          idx = idx + (*(isect1Ptr +3) > -1 ? 2 : 0);
          idx = idx + (*isect2Ptr > -1 ? 1 : 0);
          tablePtr = lineCases + idx*4;
          
          if (*tablePtr != -1)
            {
            ptIds[0] = *(isect1Ptr + *tablePtr);
            tablePtr++;
            ptIds[1] = *(isect1Ptr + *tablePtr);
            lines->InsertNextCell(2,ptIds);
          tablePtr++;
            }
          else
            {
            tablePtr += 2;
            }
          if (*tablePtr != -1)
            {
            ptIds[0] = *(isect1Ptr + *tablePtr);
            tablePtr++;
            ptIds[1] = *(isect1Ptr + *tablePtr);
            lines->InsertNextCell(2,ptIds);
            }
          }
        inPtr += inc0;
        isect2Ptr += 2;
        isect1Ptr += 2;
        }
      // now compute the last column, use s2 since it is around
      if (j < max1)
        {
        s2 = *(inPtr + dim0);
        v2 = (s2 < value ? 0 : 1);
        if (v1 ^ v2)
          {
          t = (value - s1) / (s2 - s1);
          x[axis0] = origin[axis0] + spacing[axis0]*max0;
          x[axis1] = y + spacing[axis1]*t;
          *(isect2Ptr + 1) = newPts->InsertNextPoint(x);
          if (newScalars)
            {
            newScalars->InsertNextTuple(&value);
            }
          }
        else
          {
          *(isect2Ptr + 1) = -1;
          }
        }
      }
    }

  delete [] isect1;
}

//----------------------------------------------------------------------------
//
// Contouring filter specialized for images (or slices from images)
//
void vtkSynchronizedTemplates2D::Execute()
{
  vtkImageData  *input= this->GetInput();
  vtkPointData  *pd;
  vtkPoints     *newPts;
  vtkCellArray  *newLines;
  vtkDataArray  *inScalars;
  vtkDataArray  *newScalars = NULL;
  vtkPolyData   *output = this->GetOutput();
  int           *ext;
  int           dims[3];
  int           dataSize, estimatedSize;
  

  vtkDebugMacro(<< "Executing 2D structured contour");
  
  if (input == NULL)
    {
    vtkErrorMacro("Missing input.");
    return;
    }

  ext = input->GetUpdateExtent();
  pd = input->GetPointData();
  inScalars = pd->GetScalars(this->InputScalarsSelection);
  if ( inScalars == NULL )
    {
    vtkErrorMacro(<<"Scalars must be defined for contouring");
    return;
    }

  int numComps = inScalars->GetNumberOfComponents();
  if (this->ArrayComponent >= numComps)
    {
    vtkErrorMacro("Scalars have " << numComps << " components. "
                  "ArrayComponent must be smaller than " << numComps);
    return;
    }
  
  // We have to compute the dimenisons from the update extent because
  // the extent may be larger.
  dims[0] = ext[1]-ext[0]+1;
  dims[1] = ext[3]-ext[2]+1;
  dims[2] = ext[5]-ext[4]+1;
  
  //
  // Check dimensionality of data and get appropriate form
  //
  dataSize = dims[0] * dims[1] * dims[2];

  //
  // Allocate necessary objects
  //
  estimatedSize = (int) (sqrt((double)(dataSize)));
  if (estimatedSize < 1024)
    {
    estimatedSize = 1024;
    }
  newPts = vtkPoints::New();
  newPts->Allocate(estimatedSize,estimatedSize);
  newLines = vtkCellArray::New();
  newLines->Allocate(newLines->EstimateSize(estimatedSize,2));

  //
  // Check data type and execute appropriate function
  //

  void *scalars = inScalars->GetVoidPointer(0);
  if (this->ComputeScalars)
    {
    newScalars = inScalars->NewInstance();
    newScalars->SetNumberOfComponents(inScalars->GetNumberOfComponents());
    newScalars->SetName(inScalars->GetName());
    newScalars->Allocate(5000,25000);
    }
  switch (inScalars->GetDataType())
    {
    vtkTemplateMacro5(vtkContourImage,this,(VTK_TT *)scalars, newPts,
                      newScalars, newLines);
    }//switch

  // Lets set the name of the scalars here.
  if (newScalars)
    {
    newScalars->SetName(inScalars->GetName());
    }

  vtkDebugMacro(<<"Created: " 
               << newPts->GetNumberOfPoints() << " points, " 
               << newLines->GetNumberOfCells() << " lines");
  //
  // Update ourselves.  Because we don't know up front how many lines
  // we've created, take care to reclaim memory. 
  //
  output->SetPoints(newPts);
  newPts->Delete();

  output->SetLines(newLines);
  newLines->Delete();

  if (newScalars)
    {
    output->GetPointData()->SetScalars(newScalars);
    newScalars->Delete();
    }

  output->Squeeze();
}

//----------------------------------------------------------------------------
void vtkSynchronizedTemplates2D::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os,indent);

  this->ContourValues->PrintSelf(os,indent);
  if (this->ComputeScalars)
    {
    os << indent << "ComputeScalarsOn\n";
    }
  else
    {
    os << indent << "ComputeScalarsOff\n";  
    }
  if (this->InputScalarsSelection)
    {
    os << indent << "InputScalarsSelection: " 
       << this->InputScalarsSelection << endl;
    }  
  os << indent << "ArrayComponent: " << this->ArrayComponent << endl;
}
