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

#pragma once

#include "../LayerOperator.h"

namespace NeoOnnx {

// Resize operator
class CResizeOperator : public CLayerOperator {
public:
	CResizeOperator( const onnx::NodeProto& resize, int opsetVersion );

protected:
	// CLayerOperator methods
	void AddLayers( const CTensorArray& input, CDnn& dnn, CTensorArray& outputs ) const override;

private:
	TInterpolationCoords getInterpolationCoords() const;
	TInterpolationRound getInterpolationRound() const;
	void getScales( const CTensorArray& inputs, CFastArray<float, 8>& scales ) const;
	void getSizes( const CTensorArray& inputs, CFastArray<int, 8>& sizes ) const;
};

} // namespace NeoOnnx
