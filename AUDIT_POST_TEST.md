# Auditoria pós-teste do ZConsole OS

## Escopo

Esta auditoria revisou o launcher Big Picture, a ponte local FastAPI, o empacotamento estático da Z-GameStore, o fallback Tkinter, o catálogo de 1001 itens e o workflow de composição da imagem. A revisão foi motivada pelo teste da ISO em que a loja abriu, mas os links e downloads não produziram uma ação visível.

## Achados e correções

| Área | Achado | Correção aplicada |
|---|---|---|
| Navegação externa | `window.open` dependia de popup e podia ser bloqueado no kiosk. | A loja agora usa navegação explícita na mesma aba e valida somente URLs HTTP(S). |
| Cards de jogos | O primeiro clique apenas mudava o foco; não havia ação clara para abrir a fonte. | Primeiro acionamento foca o card; segundo acionamento ou duplo clique abre a fonte oficial. |
| Catálogo local | `download_url` aponta para páginas de busca do MyAbandonware, não para arquivos ZIP. | O instalador detecta esse caso e abre a fonte oficial, sem salvar HTML como se fosse ROM. |
| Integridade do arquivo | O instalador aceitava qualquer resposta como ZIP. | Downloads diretos agora exigem assinatura ZIP válida e rejeitam caminhos inseguros no arquivo compactado. |
| Fallback Tkinter | Dialogs eram chamados a partir de thread secundária. | Mensagens voltam à thread principal por uma fila de notificações. |
| Boot | Havia divergência de `TimeoutStartSec` entre o arquivo versionado e o `build.sh`. | Ambos usam 15 segundos; a animação permanece não bloqueante e com fallback. |
| Rede | A API considerava a existência de `/sys/class/net` como conectividade. | O status exige interface ativa e rota padrão quando `ip` está disponível. |
| Overlay | Comandos indisponíveis retornavam sucesso artificial. | A API agora diferencia `ok`/`applied` e informa quando o valor foi apenas mantido na interface. |

## Limite funcional importante

O catálogo atual contém principalmente URLs de páginas de fonte. A loja não deve contornar CAPTCHA, login, anúncios, limites ou mecanismos de proteção de terceiros. Nesses casos, o comportamento correto é abrir a página oficial e deixar o usuário iniciar o download conforme os termos do site. A instalação automática só é realizada quando o catálogo receber uma URL direta para um arquivo ZIP compatível.

## Validação executada

A checagem TypeScript, a suíte Vitest e o build Vite passaram. Foram aprovados 12 testes em 5 arquivos, além da compilação dos scripts Bash e Python da imagem. O pacote estático da ISO foi sincronizado com `index.html`, CSS e JS produzidos pela última build.

## Riscos remanescentes

A ISO precisa ser reconstruída para incorporar as alterações deste relatório. O teste real de hardware ainda depende do ambiente do usuário: disponibilidade de Chromium, `xdg-open`/`gio`, `brightnessctl`, `wpctl`, `powerprofilesctl`, leitor OSCR e rota de rede. Também é necessário fornecer URLs diretas licenciadas e verificadas antes de anunciar qualquer item como instalação automática.
