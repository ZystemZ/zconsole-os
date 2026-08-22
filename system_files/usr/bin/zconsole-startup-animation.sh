#!/usr/bin/env bash
# ZConsole OS — inicialização fail-safe.
# A interface de boot usa o splash estático do Plymouth; nenhum player de vídeo
# é chamado antes da sessão gráfica, evitando tela preta em VMs e GPUs incompatíveis.
set -u

# O Plymouth e o display manager cuidam da apresentação visual. Este script
# existe para preservar o contrato do serviço antigo e sempre termina rápido.
exit 0
