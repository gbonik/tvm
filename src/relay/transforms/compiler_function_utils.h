/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file src/relay/transforms/compiler_function_utils.h
 * \brief Helper passes for working with functions with the "Compiler" attribute.
 *
 * Those wishing to use the "RelayToTIR" custom pass machinery to do IRModule-at-a-time external
 * codegen may find the following two helper passes useful:
 *
 *  - \p OutlineCompilerFunctionsWithExistingGlobalSymbols will lift inline functions with a
 *    matching "Compiler" attribute to be global functions, using the "global_symbol" attribute
 *    already assigned. Can be used before custom lowering.
 *
 *    Note that ideally "Compiler" attributed functions would be made global functions as early as
 *    possible and would stay that way. However, the GraphExecutorCodegen and AOTExecutorCodegen
 *    assume the entire model can be represented by a single 'main' function, and the Inline pass
 *    is run to respect that assumption. So this pass is mostly just to undo that Pass after modules
 *    have passed through the 'codegen' keyhole.
 *
 *    See also OutlineCompilerFunctionsMutator in src/relay/backend/contrib/ethosu/codegen.cc.
 *
 *  - (\p OutlineCompilerFunctions is a more general version of the above which can use a custom
 *    cache to both allocate "global_symbol" names and ensure two strucurally equal functions are
 *    assigned the same name, and thus lowered only once. This is used by Collage when preparing
 *    the optimally partitioned IRModule).
 *
 *  - \p MarkCompilerFunctionsAsExtern will replace global functions with a matching "Compiler"
 *    attribute with the same function with just  an "Extern" attribute, signalling the function
 *    has been dealt with. Calls to such functions will be rewritten to use the 'call_lowered'
 *    calling convention. Can be used after lowering to cleanup the IRModule.
 *
 * Note that the above behaviour is hard coded within the TECompiler, but is only available to
 * external codegen using the Function-at-a-time "relay.ext.toolchain" extension point.
 */

#ifndef TVM_RELAY_TRANSFORMS_COMPILER_FUNCTION_UTILS_H_
#define TVM_RELAY_TRANSFORMS_COMPILER_FUNCTION_UTILS_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "tvm/ir/transform.h"
#include "tvm/relay/function.h"

namespace tvm {
namespace relay {
namespace transforms {

/*!
 * \brief Abstract class representing a cache of unique global vars keyed by functions. This can
 * be used to ensure structurally equal functions are assigned the same global var object, and
 * thus lowered at most once.
 */
class GlobalSymbolCache {
 public:
  virtual ~GlobalSymbolCache();
  virtual GlobalVar GetGlobalSymbol(const Function& function) = 0;
};

/*!
 * \brief A \p GlobalSymbolCache that requires every "Compiler" attributed function to already
 * have a "global_symbol" attribute.
 */
class ExistingGlobalSymbolCache : public GlobalSymbolCache {
 public:
  ExistingGlobalSymbolCache() = default;

  GlobalVar GetGlobalSymbol(const Function& function) final;

 private:
  /*! \brief Maps already seen global symbol names to their corresponding GlobalVar objects. */
  std::unordered_map<std::string, GlobalVar> global_vars_;
};

/*!
 * \brief A pass to outline all literal functions in direct call positions which have a "Compiler"
 * attribute. The given \p GlobalSymbolCache is used to determine a unique global symbol for each
 * function, which is also assigned to the "global_symbol" attribute of the new global function.
 *
 * At most one function with the same global symbol is outlined.
 *
 * If \p compiler_filter is non-empty only functions with that as their attribute value are
 * outlined.
 */
transform::Pass OutlineCompilerFunctions(std::shared_ptr<GlobalSymbolCache> cache,
                                         std::string compiler_filter = "");

/*!
 * \brief A pass to outline all literal functions in direct call positions which have a "Compiler"
 * attribute. The functions are bound to unique global vars according to their existing
 * "global_symbol" attribute. At most one function with the same global symbol is outlined.
 *
 * If \p compiler_filter is non-empty only functions with that as their attribute value are
 * outlined.
 *
 * This pass may be useful for external codegen using the "RelayToTIR" custom pass mechanism
 * to prepare the IRModule before custom lowering.
 */
transform::Pass OutlineCompilerFunctionsWithExistingGlobalSymbols(std::string compiler_filter = "");

/*!
 * \brief A pass to mark all global functions which have a "Compiler" attribute matching
 * compiler_filter as 'extern' by replacing all attributes with a single "Extern" attribute, and
 * rewrite all calls to such functions to use the 'call_lowered' calling convention.
 *
 * If \p compiler_filter is non-empty only functions with that as their attribute value are
 * outlined.
 *
 * This pass may be useful for external codegen using the "RelayToTIR" custom pass mechanism to
 * cleanup the IRModule after custom lowering.
 */
transform::Pass MarkCompilerFunctionsAsExtern(std::string compiler_filter = "");

}  // namespace transforms
}  // namespace relay
}  // namespace tvm

#endif  // TVM_RELAY_TRANSFORMS_COMPILER_FUNCTION_UTILS_H_
