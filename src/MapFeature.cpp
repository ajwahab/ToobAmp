/*
 *   Copyright (c) 2022 Robin E. R. Davies
 *   All rights reserved.

 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 */

#include "MapFeature.h"
#include <mutex>

using namespace toob;

static LV2_URID mapFn(LV2_URID_Map_Handle handle, const char* uri)
{
	MapFeature* feature = (MapFeature*)(void*)handle;
	return feature->GetUrid(uri);
}

MapFeature::MapFeature()
{
	feature.URI = LV2_URID__map;
	feature.data = &map;
	map.handle = (void*)this;
	map.map = &mapFn;
}

LV2_URID MapFeature::GetUrid(const char* uri)
{

	std::lock_guard<std::mutex> guard(mapMutex);

	LV2_URID result = stdMap[uri];
	if (result == 0)
	{
		stdMap[uri] = ++nextAtom;
		result = nextAtom;
	}
	return result;
}
