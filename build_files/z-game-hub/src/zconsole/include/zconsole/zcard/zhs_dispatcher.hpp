#pragma once
#include "zconsole/zcard/zhs_device.hpp"
#include <memory>
#include <string>
#include <vector>

namespace zconsole {

class ZhsDispatcher {
public:
  explicit ZhsDispatcher(ZhsDevice& device) : device_(device) {}

  // Recebe um quadro completo e devolve um quadro de resposta completo.
  // O comando e o request_id são preservados; flags contém ZhsStatus.
  std::vector<std::uint8_t> handle(const std::vector<std::uint8_t>& request,
                                   std::string* error = nullptr);

private:
  ZhsDevice& device_;
};

} // namespace zconsole
