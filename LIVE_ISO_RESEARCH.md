# Live ISO e Anaconda — notas de implementação

A documentação do Red Hat descreve `anaconda-iso` como uma ISO inicializável de instalação criada a partir de uma imagem bootc; ela não deve ser chamada automaticamente de Live Desktop. Referência: https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/10/html/using_image_mode_for_rhel_to_build_deploy_and_manage_operating_systems/creating-bootc-compatible-base-disk-images-by-using-bootc-image-builder

O guia de instalação do Bazzite descreve um Live Environment que permite testar o desktop antes de instalar. Referência: https://docs.bazzite.gg/General/Installation_Guide/install-guide/

Conclusão para o ZConsole: manter `anaconda-iso` como mídia instalável e adicionar uma etapa separada de Live ISO somente se houver uma composição Fedora/Lorax validada. Não rotular a Anaconda ISO atual como LiveCD sem confirmar que ela inicia uma sessão desktop. A correção imediata deve priorizar a tela preta pós-instalação, usando logs da máquina de teste, display manager habilitado, alvo gráfico, launcher iniciado após a sessão e fallback acessível por TTY.
