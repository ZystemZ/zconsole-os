#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace zconsole {

enum class RomBackendKind { InternalCore, ExternalProcess, WindowsMount, Unavailable };

struct RomRuntimeConfig {
  std::string rom;
  std::string system;
  bool internalEnabled = false;
  std::string internalCore;
  bool externalEnabled = false;
  std::filesystem::path externalCommand;
  std::vector<std::string> externalArguments;
};

struct RomLaunchPlan {
  RomBackendKind backend = RomBackendKind::Unavailable;
  std::string system;
  std::string internalCore;
  std::filesystem::path romPath;
  std::filesystem::path command;
  std::vector<std::string> arguments;
  std::string mountMode;
  std::string message;
};

struct RomProcessInvocation {
  std::filesystem::path executable;
  std::vector<std::string> arguments;
};

bool resolveRomRuntime(const std::filesystem::path& cardRoot,
                      const std::filesystem::path& romPath,
                      RomLaunchPlan& plan,
                      std::string* error = nullptr);

// Prepara argv sem shell e sem iniciar o processo. {rom} é substituído pelo
// caminho absoluto da ROM; argumentos não são interpretados como comandos.
bool prepareExternalRomInvocation(const RomLaunchPlan& plan,
                                  RomProcessInvocation& invocation,
                                  std::string* error = nullptr);

// Inicia o processo configurado sem passar por shell. Em hosts sem suporte
// nativo, retorna erro explícito em vez de executar uma string arbitrária.
bool launchExternalRom(const RomLaunchPlan& plan, std::string* error = nullptr);

const char* toString(RomBackendKind backend);

} // namespace zconsole
