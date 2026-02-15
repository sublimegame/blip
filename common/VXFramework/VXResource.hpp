//
//  VXResource.hpp
//  Cubzh
//
//  Created by Adrien Duermael on 15/10/2020.
//

#pragma once

// C++
#include <string>

#ifdef CLIENT_ENV

/// Saves the item file and computes its etag
/// Returns true on success
bool saveItemInCache(const std::string &repo,
                     const std::string &name,
                     const std::string &pcubes);

/// Updates the .3zh.cache file for an item.
/// It update the
bool updateItemCacheInfo(const std::string& repo,
                         const std::string& name,
                         std::string * const etag,
                         const int32_t timestamp);

#endif
