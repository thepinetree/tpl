#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/MemoryBuffer.h"

#include "util/common.h"
#include "util/macros.h"

//
// This is a stand-alone executable that reads a bitcode file, cleans, and
// optimizes it to improve JIT compilation times. The input bitcode file is
// generated during a built of TPL and contains all the bytecode handler logic
// for TPL bytecode/opcodes.
//
// Background: to generate a bitcode file with **JUST** the bytecode handler
// logic, we define a static variable array that stores function pointers to all
// bytecode handler functions in a separate CPP file independent from TPL. The
// global variable forces the definitions of those functions in the bitcode
// file. The drawback of this approach is that unused bytecode functions cannot
// be removed or optimized out during JIT since they are force-used. This
// dramatically slows down optimization/compilation since DCE can't do anything.
// Moreover, the generated functions have DSO local linkage, which should
// ideally be LinkOnce ODR.
//
// Hence, this executable is run on the generated unoptimized bitcode to clean
// it up.
//
// This executable reads the unoptimized bitcode file and:
// 1. Converts to an LLVM Module
// 2. Removes the static global variable
// 3. Modifies linkage types of all defined functions to LinkOnce
// 4. Cleans up function arguments
// 5. Writes out optimized module as bitcode file
//

static constexpr const char *kGlobalVarName = "kAllFuncs";
static constexpr const char *kLLVMCompiledUsed = "llvm.compiler.used";

auto ReadIntoMemory(const char *filepath) {
  auto memory_buffer = llvm::MemoryBuffer::getFile(filepath);
  if (auto error = memory_buffer.getError()) {
    fprintf(stderr, "Error loading bytecode handler bitcode file: %s\n",
            error.message().c_str());
    exit(1);
  }

  return memory_buffer;
}

auto ParseIntoLLVMModule(llvm::LLVMContext *ctx, llvm::MemoryBuffer *buffer) {
  auto module = llvm::parseBitcodeFile(*buffer, *ctx);
  if (!module) {
    auto error = llvm::toString(module.takeError());
    fprintf(stderr, "Error parsing bytecode handler bitcode file: %s\n",
            error.c_str());
    exit(1);
  }

  return module;
}

void RemoveGlobalUses(llvm::Module *module) {
  // When we created the original bitcode file, we forced all functions to be
  // generated by storing their address in a global variable. We delete this
  // variable now so the final binary can be made smaller by eliminating unused
  // ops.
  auto var = module->getGlobalVariable(kGlobalVarName);
  if (var != nullptr) {
    var->replaceAllUsesWith(llvm::UndefValue::get(var->getType()));
    var->eraseFromParent();
  }

  // Clang created a global variable holding all force-used items. Delete it.
  auto used = module->getGlobalVariable(kLLVMCompiledUsed);
  if (used != nullptr) {
    used->eraseFromParent();
  }
}

void CleanFunctions(llvm::Module *module) {
  for (auto &func : *module) {
    if (!func.empty()) {
      func.setLinkage(llvm::GlobalValue::LinkageTypes::LinkOnceODRLinkage);
    }
    func.setDSOLocal(false);
  }
}

void CleanModule(llvm::Module *module) {
  // Clean out the name
  module->setSourceFileName("");

  // Remove all globals
  RemoveGlobalUses(module);

  // Clean functions
  CleanFunctions(module);
}

void WriteCleanedModule(llvm::Module *module, const char *out_filepath) {
  std::error_code error;
  llvm::raw_fd_ostream file_stream(out_filepath, error);

  if (error) {
    fprintf(stderr, "Error opening output file '%s': %s\n", out_filepath,
            error.message().c_str());
    exit(1);
  }

  llvm::WriteBitcodeToFile(*module, file_stream);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: gen_bc <input_bc>\n");
    exit(1);
  }

  const char *input_filepath = argv[1];
  const char *output_filepath = (argc > 2 ? argv[2] : "./bytecode_handlers.bc");

  fprintf(stdout, "Reading input '%s' ...\n", input_filepath);

  auto memory_buffer = ReadIntoMemory(input_filepath);

  fprintf(stdout, "Parsing into LLVM Module ...\n");

  auto context = std::make_unique<llvm::LLVMContext>();
  auto module = ParseIntoLLVMModule(context.get(), memory_buffer->get());

  fprintf(stdout, "Cleaning up LLVM Module ...\n");

  CleanModule(module->get());

  fprintf(stdout, "Writing cleaned LLVM Module ...\n");

  WriteCleanedModule(module->get(), output_filepath);
}
