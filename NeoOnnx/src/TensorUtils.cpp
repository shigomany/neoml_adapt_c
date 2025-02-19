/* Copyright © 2017-2020 ABBYY Production LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
--------------------------------------------------------------------------------------------------------------*/

#include "common.h"
#pragma hdrstop

#include <cfloat>
#include <cmath>

#include "NeoOnnxCheck.h"
#include "TensorUtils.h"

namespace NeoOnnx {

bool IsInteger( float x )
{
	return std::fabs( std::roundf( x ) - x ) < FLT_EPSILON;
}

TBlobType GetBlobType( const onnx::TensorProto_DataType& onnxDataType )
{
	switch( onnxDataType ) {
		case onnx::TensorProto::FLOAT:
		case onnx::TensorProto::DOUBLE:
			return CT_Float;
		case onnx::TensorProto::BOOL:
		case onnx::TensorProto::INT8:
		case onnx::TensorProto::UINT8:
		case onnx::TensorProto::INT16:
		case onnx::TensorProto::UINT16:
		case onnx::TensorProto::INT32:
		case onnx::TensorProto::UINT32:
		// Sometimes Constant operator's value is stored in int64 (even if it can be stored in 32-bit integer)
		// That's why we allow here to use int64 and will cast it later to 32-bit
		case onnx::TensorProto::INT64:
		case onnx::TensorProto::UINT64:
			return CT_Int;
		case onnx::TensorProto::FLOAT16:
		case onnx::TensorProto::BFLOAT16:
		case onnx::TensorProto::COMPLEX64:
		case onnx::TensorProto::COMPLEX128:
		case onnx::TensorProto::UNDEFINED:
		default:
			CheckNeoOnnxSupport( false, "tensor type" );
	}
	return CT_Invalid;
}

//---------------------------------------------------------------------------------------------------------------------

// Gets layer name with the given prefix which isn't used in dnn
static CString getUniqueLayerName( const CDnn& dnn, const CString& prefix )
{
	int currIndex = dnn.GetLayerCount();
	CString currName = prefix + Str( currIndex );
	while( dnn.HasLayer( currName ) ) {
		++currIndex;
		currName = prefix + Str( currIndex );
	}
	return currName;
}

// Converts tensor in a way that layout[heightDimIndex] is BD_Height and layout[widthDimIndex] is BD_Width.
// If widthDimIndex is NotFound then only height dimension is moved to layout[heightDimIndex]
static CPtr<const CUserTensor> convertTensorToHw( const CUserTensor& input, int heightDimIndex, int widthDimIndex )
{
	const CTensorLayout& inputLayout = input.Layout();

	if( inputLayout[heightDimIndex] == BD_Height
		&& ( widthDimIndex == NotFound || inputLayout[widthDimIndex] == static_cast<int>( BD_Width ) ) )
	{
		return &input;
	}

	CTensorLayout newLayout;
	newLayout.SetBufferSize( input.DimCount() );
	TBlobDim unusedAxis = BD_BatchLength;
	for( int i = 0; i < input.DimCount(); ++i ) {
		if( i == heightDimIndex ) {
			newLayout.Add( BD_Height );
		} else if( i == widthDimIndex ) {
			newLayout.Add( BD_Width );
		} else if( widthDimIndex == NotFound ) {
			newLayout.Add( unusedAxis < BD_Height ? unusedAxis : unusedAxis + 1 );
			++unusedAxis;
		} else {
			newLayout.Add( unusedAxis < BD_Height ? unusedAxis : unusedAxis + 2 );
			++unusedAxis;
		}
	}

	NeoAssert( newLayout[heightDimIndex] == BD_Height );
	NeoAssert( widthDimIndex == -1 || newLayout[widthDimIndex] == BD_Width );
	return ConvertTensor( input, newLayout );
}

//---------------------------------------------------------------------------------------------------------------------

// Swaps 2 dimensions of data blob
static CPtr<const CDnnBlob> swapDimensions( const CDnnBlob& inputBlob, TBlobDim firstDim, TBlobDim secondDim )
{
	CBlobDesc outputDesc = inputBlob.GetDesc();
	const int firstDimSize = outputDesc.DimSize( firstDim );
	const int secondDimSize = outputDesc.DimSize( secondDim );
	outputDesc.SetDimSize( firstDim, secondDimSize );
	outputDesc.SetDimSize( secondDim, firstDimSize );

	IMathEngine& mathEngine = inputBlob.GetMathEngine();
	CPtr<CDnnBlob> outputBlob = CDnnBlob::CreateBlob( mathEngine, inputBlob.GetDataType(), outputDesc );
	outputBlob->TransposeFrom( &inputBlob, firstDim, secondDim );
	return outputBlob.Ptr();
}

// Swaps 2 dimensions of given layer output
static CLayerOutput swapDimensions( const CLayerOutput& input, TBlobDim firstDim, TBlobDim secondDim )
{
	CDnn& dnn = *( input.Layer->GetDnn() );
	CPtr<CTransposeLayer> transposeLayer = new CTransposeLayer( dnn.GetMathEngine() );
	transposeLayer->SetName( getUniqueLayerName( dnn, "transpose_" ) );
	transposeLayer->SetTransposedDimensions( firstDim, secondDim );
	dnn.AddLayer( *transposeLayer );
	transposeLayer->Connect( 0, *input.Layer, input.OutputIndex );
	return CLayerOutput( transposeLayer.Ptr(), 0 );
}

// Swaps 2 dimensions of input tensor
static CPtr<const CTensorBase> swapDimensions( const CTensorBase& input, TBlobDim firstDim, TBlobDim secondDim )
{
	CTensorLayout outputLayout = input.Layout();
	const int firstDimIndex = outputLayout.Find( firstDim );
	const int secondDimIndex = outputLayout.Find( secondDim );
	NeoAssert( firstDimIndex != NotFound );
	if( secondDimIndex != NotFound ) {
		swap( outputLayout[firstDimIndex], outputLayout[secondDimIndex] );
	} else {
		outputLayout[firstDimIndex] = secondDim;
	}

	if( input.IsCalculated() ) {
		CPtr<const CDnnBlob> blob = swapDimensions( *dynamic_cast<const CDataTensor&>( input ).Data(),
			firstDim, secondDim );
		return new CDataTensor( input.Shape(), outputLayout, *blob );
	}

	CLayerOutput layerOutput = swapDimensions( dynamic_cast<const CUserTensor&>( input ).LayerOutput(),
		firstDim, secondDim );
	return new CUserTensor( input.Shape(), outputLayout, layerOutput );
}

CPtr<const CTensorBase> ConvertTensor( const CTensorBase& input, const CTensorLayout& outputLayout )
{
	// Trivial case
	if( input.Layout() == outputLayout ) {
		return &input;
	}

	const int dimCount = outputLayout.Size();
	NeoAssert( input.DimCount() == dimCount );

	CPtr<const CTensorBase> currentTensor = &input;
	for( int dimIndex = 0; dimIndex < dimCount; ++dimIndex ) {
		TBlobDim inputDim = currentTensor->Layout()[dimIndex];
		TBlobDim outputDim = outputLayout[dimIndex];
		if( inputDim != outputDim ) {
			currentTensor = swapDimensions( *currentTensor, inputDim, outputDim );
		}
	}

	return currentTensor;
}

CPtr<const CDataTensor> ConvertTensor( const CDataTensor& dataTensor, const CTensorLayout& destLayout )
{
	return dynamic_cast<const CDataTensor*>( ConvertTensor( static_cast<const CTensorBase&>( dataTensor ), destLayout ).Ptr() );
}

CPtr<const CUserTensor> ConvertTensor( const CUserTensor& dataTensor, const CTensorLayout& destLayout )
{
	return dynamic_cast<const CUserTensor*>( ConvertTensor( static_cast<const CTensorBase&>( dataTensor ), destLayout ).Ptr() );
}

// --------------------------------------------------------------------------------------------------------------------

void CalculatePadding( const CString& autoPad, const CTensorShape& kernelShape, CFastArray<int, 8>& pads )
{
	const int padDims = static_cast<int>( kernelShape.Size() );
	for( int padDimIndex = 0; padDimIndex < padDims; ++padDimIndex ) {
		const int totalPadSize = kernelShape[padDimIndex] - 1;
		if( autoPad == "SAME_LOWER" ) {
			pads[padDimIndex] = ( totalPadSize + 1 ) / 2;
		} else {
			pads[padDimIndex] = totalPadSize / 2;
		}
		pads[padDims + padDimIndex] = totalPadSize - pads[padDimIndex];
	}
}

// --------------------------------------------------------------------------------------------------------------------

// Adds given image resize layer in order to resize heightDimIndex'th and widthDimIndex'th dimensions
// widthDimIndex may be NotFound (that means only heightDimIndex'th dimension should be resized)
static CPtr<const CUserTensor> addImageResizeLayer( CImageResizeLayer& imageResize, CDnn& dnn, const CUserTensor& input,
	int heightDimIndex, int widthDimIndex )
{
	// Add imageResize layer
	CPtr<const CUserTensor> result = convertTensorToHw( input, heightDimIndex, widthDimIndex );
	imageResize.Connect( 0, *result->Layer(), result->OutputIndex() );
	dnn.AddLayer( imageResize );

	// Calculate output shape
	CTensorShape outputShape;
	result->Shape().CopyTo( outputShape );
	outputShape[heightDimIndex] += imageResize.GetDelta( CImageResizeLayer::IS_Top )
		+ imageResize.GetDelta( CImageResizeLayer::IS_Bottom );
	if( widthDimIndex != NotFound ) {
		outputShape[widthDimIndex] += imageResize.GetDelta( CImageResizeLayer::IS_Left )
			+ imageResize.GetDelta( CImageResizeLayer::IS_Right );
	}

	// Construct new CUserTensor which is provided by imageResize layer
	return new CUserTensor( outputShape, result->Layout(), CLayerOutput( &imageResize, 0 ) );
}

CPtr<const CUserTensor> PadUserTensor( const CUserTensor& input, const CFastArray<int, 8>& pads, float padValue )
{
	// Pool and conv operators storing pads only for N-2 tensor dimensions (leaving out batch and channels)
	// On the other side Pad operator is storing pads for every tensor dimension

	// Number of padded dimensions
	const int paddedDims = pads.Size() / 2;
	NeoAssert( pads.Size() == paddedDims * 2 );
	// Index of first padded dimension
	const int padDimIndex = input.DimCount() - paddedDims;
	// Prefix for padding layer names
	const CString padNamePrefix = input.Layer()->GetName() + CString( "_pad_" );
	// Used network
	CDnn& dnn = *( input.Layer()->GetDnn() );
	// Used mathEngine
	IMathEngine& mathEngine = dnn.GetMathEngine();

	CPtr<const CUserTensor> currData = &input;
	CPtr<CImageResizeLayer> imageResize = nullptr;
	int heightDimIndex = NotFound;
	int widthDimIndex = NotFound;

	for( int i = 0; i < paddedDims; ++i ) {
		if( pads[i] == 0 && pads[i + paddedDims] == 0 ) {
			continue;
		}

		if( imageResize == nullptr ) {
			imageResize = new CImageResizeLayer( mathEngine );
			imageResize->SetName( getUniqueLayerName( dnn, padNamePrefix ) );
			imageResize->SetDefaultValue( padValue );
		}

		if( heightDimIndex == NotFound ) {
			heightDimIndex = padDimIndex + i;
			imageResize->SetDelta( CImageResizeLayer::IS_Top, pads[i] );
			imageResize->SetDelta( CImageResizeLayer::IS_Bottom, pads[paddedDims + i] );
		} else {
			widthDimIndex = padDimIndex + i;
			imageResize->SetDelta( CImageResizeLayer::IS_Left, pads[i] );
			imageResize->SetDelta( CImageResizeLayer::IS_Right, pads[paddedDims + i] );
			currData = addImageResizeLayer( *imageResize, dnn, *currData, heightDimIndex, widthDimIndex );
			imageResize = nullptr;
			heightDimIndex = NotFound;
			widthDimIndex = NotFound;
		}
	}

	// In case of padding odd number of dimensions by this moment imageResize != nullptr
	// and widthDimIndex is equal to NotFound
	if( imageResize != nullptr ) {
		currData = addImageResizeLayer( *imageResize, dnn, *currData, heightDimIndex, widthDimIndex );
	}

	return currData;
}

//---------------------------------------------------------------------------------------------------------------------

// Returns true if shapes are equal
static bool areShapesEqual( const CTensorShape& first, const CTensorShape& second )
{
	if( first.Size() != second.Size() ) {
		return false;
	}

	for( int i = 0; i < first.Size(); ++i ) {
		if( first[i] != second[i] ) {
			return false;
		}
	}

	return true;
}

bool BroadcastTensorShape( const CTensorShape& first, const CTensorShape& second, const CBroadcast& broadcast, CTensorShape& result )
{
	if( broadcast.Type == BT_None ) {
		// No broadcast, the shape must match
		if( areShapesEqual( first, second ) ) {
			first.CopyTo( result );
			return true;
		}
		return false;
	}

	int axis = NotFound;
	if( broadcast.Type == BT_Onnx ) {
		axis = broadcast.Axis;
		CheckNeoOnnxSupport( second.Size() <= first.Size(), "second tensor has more dimensions" );
		if( axis < 0 ) {
			axis = abs( first.Size() - second.Size() );
		}
	} else {
		// Numpy-style broadcast is similar to the Onnx-broadcast with axis equal to difference
		// in number of dimensions
		axis = abs( first.Size() - second.Size() );
	}

	// The shape with lesser number of dimensions must be padded
	const CTensorShape& lesserShape = first.Size() <= second.Size() ? first : second;
	const CTensorShape& biggerShape = first.Size() > second.Size() ? first  : second;
	CTensorShape paddedShape;
	paddedShape.Add( 1, axis );
	paddedShape.Add( lesserShape );
	if( paddedShape.Size() > biggerShape.Size() ) {
		// Wrong broadcast parameters (axis value is too big)
		return false;
	}
	NeoAssert( broadcast.Type == BT_Onnx || paddedShape.Size() == biggerShape.Size() );

	// This will add ones only in case of BT_Onnx and axis != abs( first.Size() - second.Size() )
	paddedShape.Add( 1, biggerShape.Size() - paddedShape.Size() );

	result.SetSize( paddedShape.Size() );
	for( int dim = 0; dim < result.Size(); ++dim ) {
		if( paddedShape[dim] == biggerShape[dim] || min( paddedShape[dim], biggerShape[dim] ) == 1 ) {
			result[dim] = max( paddedShape[dim], biggerShape[dim] );
		} else {
			result.DeleteAll();
			return false;
		}
	}

	return true;
}

// Adds upsample layer to the dnn
static CPtr<const CUserTensor> addUpsample2dLayer( CUpsampling2DLayer& upsample, CDnn& dnn, const CUserTensor& input,
	int heightDimIndex, int widthDimIndex )
{
	// Add imageResize layer
	CPtr<const CUserTensor> result = convertTensorToHw( input, heightDimIndex, widthDimIndex );
	upsample.Connect( 0, *result->Layer(), result->OutputIndex() );
	dnn.AddLayer( upsample );

	// Calculate output shape
	CTensorShape outputShape;
	result->Shape().CopyTo( outputShape );

	outputShape[heightDimIndex] *= upsample.GetHeightCopyCount();
	if( widthDimIndex != NotFound ) {
		outputShape[widthDimIndex] *= upsample.GetWidthCopyCount();
	}

	// Construct new CUserTensor which is provided by imageResize layer
	return new CUserTensor( outputShape, result->Layout(), CLayerOutput( &upsample, 0 ) );
}

CPtr<const CUserTensor> PrepareForBroadcast( const CUserTensor& input, const CBroadcast& broadcast, int outputDims )
{
	int axis = outputDims - input.DimCount();
	if( broadcast.Type == BT_Onnx && broadcast.Axis >= 0 && axis > broadcast.Axis ) {
		axis = broadcast.Axis;
	}

	const CTensorShape& inputShape = input.Shape();
	NeoAssert( axis + inputShape.Size() <= outputDims );

	CTensorShape outputShape;
	outputShape.Add( 1, axis );
	outputShape.Add( inputShape );
	outputShape.Add( 1, outputDims - outputShape.Size() );

	const CTensorLayout& inputLayout = input.Layout();

	TBlobDim currDim = BD_BatchLength;
	CTensorLayout outputLayout;
	outputLayout.SetBufferSize( outputDims );
	// Adding unused blob dims to the new layout
	for( int i = 0; i < axis; ++i ) {
		while( inputLayout.Find( currDim ) != NotFound && currDim < BD_Count ) {
			++currDim;
		}
		NeoAssert( currDim != BD_Count );
		outputLayout.Add( currDim );
		++currDim;
	}
	// Copying existing dims
	outputLayout.Add( inputLayout );
	// Adding unused blob dims to the new layout
	for( int i = outputLayout.Size(); i < outputDims; ++i ) {
		while( inputLayout.Find( currDim ) != NotFound && currDim < BD_Count ) {
			++currDim;
		}
		NeoAssert( currDim != BD_Count );
		outputLayout.Add( currDim );
		++currDim;
	}

	return new CUserTensor( outputShape, outputLayout, input.LayerOutput() );
}

// Broadcasts user tensor into outputShape via broadcastInfo
static CPtr<const CUserTensor> broadcastUserTensor( const CUserTensor& input, const CBroadcast& broadcast,
	const CTensorShape& outputShape )
{
	if( areShapesEqual( input.Shape(), outputShape ) ) {
		return &input;
	}

	NeoAssert( broadcast.Type != BT_None );
	NeoAssert( input.DimCount() <= outputShape.Size() );
	NeoAssert( broadcast.Type != BT_Upsample || input.DimCount() == outputShape.Size() );

	// Prefix for upsample layer names
	const CString upsampleNamePrefix = input.Layer()->GetName() + CString( "_upsample_" );
	// Used network
	CDnn& dnn = *( input.Layer()->GetDnn() );
	// Used mathEngine
	IMathEngine& mathEngine = dnn.GetMathEngine();

	CPtr<const CUserTensor> currData = PrepareForBroadcast( input, broadcast, outputShape.Size() );
	CPtr<CUpsampling2DLayer> upsample = nullptr;
	int heightDimIndex = NotFound;
	int widthDimIndex = NotFound;
	CTensorShape inputShape;
	currData->Shape().CopyTo( inputShape );

	for( int i = 0; i < inputShape.Size(); ++i ) {
		if( inputShape[i] == outputShape[i] ) {
			continue;
		}
		NeoAssert( broadcast.Type == BT_Upsample || inputShape[i] == 1 );
		NeoAssert( outputShape[i] % inputShape[i] == 0 );

		if( upsample == nullptr ) {
			upsample = new CUpsampling2DLayer( mathEngine );
			upsample->SetName( getUniqueLayerName( dnn, upsampleNamePrefix ) );
		}

		if( heightDimIndex == NotFound ) {
			heightDimIndex = i;
			upsample->SetHeightCopyCount( outputShape[i] / inputShape[i] );
		} else {
			widthDimIndex = i;
			upsample->SetWidthCopyCount( outputShape[i] / inputShape[i] );
			currData = addUpsample2dLayer( *upsample, dnn, *currData, heightDimIndex, widthDimIndex );
			upsample = nullptr;
			heightDimIndex = NotFound;
			widthDimIndex = NotFound;
		}
	}

	// In case of broadcasting odd number of dimensions by this moment upsample != nullptr
	// widthDimIndex is equal to NotFound
	if( upsample != nullptr ) {
		// Default value is 0 which is invalid
		upsample->SetWidthCopyCount( 1 );
		currData = addUpsample2dLayer( *upsample, dnn, *currData, heightDimIndex, widthDimIndex );
	}

	return currData;
}

// Broadcasts data tensor into outputShape via broadcastInfo
static CPtr<const CDataTensor> broadcastDataTensor( const CDataTensor& input, const CBroadcast& broadcast,
	const CTensorShape& outputShape )
{
	if( areShapesEqual( input.Shape(), outputShape ) ) {
		return &input;
	}

	NeoAssert( broadcast.Type != BT_None );

	// The broadcast of data tensor is done by building temporary dnn which broadcasts user tensor
	// and running this dnn on the data from the input
	IMathEngine& mathEngine = input.Data()->GetMathEngine();
	CRandom random( 0x32456 );

	CDnn internalDnn( random, mathEngine );
	// Create user tensor of the same shape linked to the source layer of the internal dnn
	CPtr<const CUserTensor> internalInput = AsUserTensor( input, "BroadcastSource", internalDnn );

	// Broadcast user tensor
	// This step adds broadcasting layers to the internal dnn
	CPtr<const CUserTensor> internalOutput = broadcastUserTensor( *internalInput, broadcast, outputShape );
	NeoPresume( areShapesEqual( internalOutput->Shape(), outputShape ) );

	// Add sink which will be used to extract broadcasted data from the internal dnn
	CPtr<CSinkLayer> sink = new CSinkLayer( mathEngine );
	sink->Connect( 0, *internalOutput->Layer(), internalOutput->OutputIndex() );
	internalDnn.AddLayer( *sink );

	// Run dnn on the data from the input
	internalDnn.RunOnce();

	// Create new data tensor with the blob from the internal dnn sink
	return new CDataTensor( outputShape, internalOutput->Layout(), *sink->GetBlob() );
}

CPtr<const CTensorBase> BroadcastTensor( const CTensorBase& input, const CBroadcast& broadcast,
	const CTensorShape& outputShape )
{
	if( input.IsCalculated() ) {
		return broadcastDataTensor( dynamic_cast<const CDataTensor&>( input ), broadcast, outputShape ).Ptr();
	}
	return broadcastUserTensor( dynamic_cast<const CUserTensor&>( input ), broadcast, outputShape ).Ptr();
}

//---------------------------------------------------------------------------------------------------------------------

CPtr<const CUserTensor> AsUserTensor( const CTensorBase& tensor, const CString& layerName, CDnn& dnn )
{
	if( !tensor.IsCalculated() ) {
		// No conversion needed
		return dynamic_cast<const CUserTensor*>( &tensor );
	}

	const CDataTensor& dataTensor = dynamic_cast<const CDataTensor&>( tensor );
	CPtr<CDataLayer> dataLayer = new CDataLayer( dnn.GetMathEngine() );
	dataLayer->SetBlob( dataTensor.Data()->GetCopy() );
	// Guarantee that serialization won't lead to data loss
	dataLayer->SetName( layerName );
	dnn.AddLayer( *dataLayer );
	return new CUserTensor( dataTensor.Shape(), dataTensor.Layout(), CLayerOutput( dataLayer, 0 ) );
}

} // namespace NeoOnnx
