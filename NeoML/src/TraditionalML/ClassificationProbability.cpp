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

#include <common.h>
#pragma hdrstop

#include <NeoML/TraditionalML/ClassificationProbability.h>

namespace NeoML {

const double CClassificationProbability::minValue = 0.0;
const double CClassificationProbability::maxValue = 1.0;
const double CClassificationProbability::precision = 1E-6;
const CClassificationProbability CClassificationProbability::minProbability = CClassificationProbability( minValue );
const CClassificationProbability CClassificationProbability::maxProbability = CClassificationProbability( maxValue );

const CClassificationProbability& CClassificationProbability::MinProbability()
{
	return minProbability;
}

const CClassificationProbability& CClassificationProbability::MaxProbability()
{
	return maxProbability;
}

} // namespace NeoML

// Implementation C Interface

using namespace NeoML;

// --- Contructors --- //

void *CClassificationProbabilityInit(double value) {
	return new CClassificationProbability(value);
}

// --- Contructors --- //

// --- Functions --- //

double CClassificationProbabilityGetValue(void *ptr) {
	auto instance = static_cast<CClassificationProbability*>(ptr);
	return instance->GetValue();
}

void CClassificationProbabilitySetValue(void *ptr, double value) {
	auto instance = static_cast<CClassificationProbability*>(ptr);
	return instance->SetValue(value);
}

bool CClassificationProbabilityIsValid(void *ptr) {
	auto instance = static_cast<CClassificationProbability*>(ptr);
	return instance->IsValid();
}

// --- Functions --- //