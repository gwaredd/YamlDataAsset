#ifndef YAML_H_62B23520_7C8E_11DE_8A39_0800200C9A66
#define YAML_H_62B23520_7C8E_11DE_8A39_0800200C9A66

#if defined(_MSC_VER) ||                                            \
    (defined(__GNUC__) && (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) || \
     (__GNUC__ >= 4))  // GCC supports "pragma once" correctly since 3.4
#pragma once
#endif

#include "yaml-cpp/include/parser.h"
#include "yaml-cpp/include/emitter.h"
#include "yaml-cpp/include/emitterstyle.h"
#include "yaml-cpp/include/stlemitter.h"
#include "yaml-cpp/include/exceptions.h"
#include "yaml-cpp/include/node/node.h"
#include "yaml-cpp/include/node/impl.h"
#include "yaml-cpp/include/node/convert.h"
#include "yaml-cpp/include/node/iterator.h"
#include "yaml-cpp/include/node/detail/impl.h"
#include "yaml-cpp/include/node/parse.h"
#include "yaml-cpp/include/node/emit.h"

#endif  // YAML_H_62B23520_7C8E_11DE_8A39_0800200C9A66
