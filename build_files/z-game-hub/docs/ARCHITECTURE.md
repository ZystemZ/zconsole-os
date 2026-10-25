# Arquitetura

## Camadas
- UI Qt Quick/QML: apresentação e navegação.
- GameCatalog: catálogo SQLite e operações de cadastro.
- Scanner: busca de candidatos em diretórios configurados.
- Launcher: ponto inicial para integrações de execução; no protótipo aceita apenas um executável sem argumentos.
- Próximos módulos: adaptadores por launcher, dispositivos UDisks2/D-Bus, metadados e downloads autorizados.

## Regras
- A varredura inicial não cadastra automaticamente cada arquivo: extensões ambíguas e executáveis auxiliares exigem classificação.
- Desconectar uma unidade não deve apagar jogos do catálogo.
- Comandos de execução não podem vir de páginas web sem validação e confirmação.
- A loja deve priorizar páginas oficiais, licenças explícitas e conteúdo autorizado.

## v0.4: seleção gráfica

`QtQuick.Dialogs.FileDialog` permite escolher um arquivo, múltiplos executáveis/ROMs ou uma
pasta. `GameCatalog::localPathFromUrl` converte URLs `file://` em caminhos locais, e
`GameCatalog::addPath` valida o caminho e deriva um título inicial do nome selecionado antes de
gravar a entrada como `manual`. A seleção não executa o conteúdo; a configuração de RetroArch,
Proton, Lutris, Heroic e Bottles permanecem separada.

A biblioteca também expõe `updateManualGame`; a interface reutiliza o diálogo de cadastro para
editar registros manuais e pede confirmação antes de removê-los. Registros Steam e de outras
fontes continuam protegidos contra edição acidental. Pesquisa textual e filtro de plataforma são
aplicados no modelo visual sem alterar os dados persistidos.

## v0.5: emuladores e RetroArch

A tabela `emulator_configs` armazena um executável, argumentos, core opcional e estado por
plataforma. A tela de configuração detecta executáveis `retroarch` disponíveis no `PATH` e
permite salvar também outros emuladores. Ao iniciar um registro sem comando explícito, o
catálogo procura a configuração da plataforma, substitui `{ROM}` e `{CORE}` nos argumentos e
anexa a ROM quando o marcador não foi informado. O processo é iniciado com
`QProcess::startDetached`, nunca por shell.

O core é opcional: quando informado sem `{CORE}`, o lançador acrescenta `-L <core>`, padrão
compatível com o RetroArch. A validação em máquina real ainda é necessária para confirmar os
nomes dos cores, instalações Flatpak e permissões de acesso aos diretórios de ROM.

Quando o executável configurado é um caminho existente, ele é preservado como um único
argumento mesmo que contenha espaços. Comandos compostos como `flatpak run
org.libretro.RetroArch` são divididos em programa e argumentos sem shell, mantendo o mesmo
modelo seguro de execução.

O catálogo procura arquivos `*_libretro.so` em diretórios comuns do RetroArch nativo e Flatpak.
Para NES, SNES, Game Boy, Mega Drive, Nintendo 64 e PlayStation há uma ordem de preferência
de nomes; a sugestão só é preenchida quando um core instalado correspondente é encontrado.

## v0.6: launchers Windows e presets

O scanner lê atalhos `.desktop` em diretórios de aplicações do usuário, Flatpak e sistema. Itens
que mencionam Lutris ou Heroic são apresentados para revisão, preservando o `Exec` original como
comando de lançamento e identificando a origem como `Lutris` ou `Heroic`. A importação é sempre
explícita. Jogos Steam continuam usando a URI `steam://rungameid`, portanto a Steam mantém o
controle do Proton e dos prefixos configurados.

Configurações de emulador agora incluem preset, proporção de tela e migração automática das
colunas novas no SQLite. Presets geram argumentos RetroArch previsíveis; argumentos manuais têm
prioridade, e a proporção só é acrescentada quando não existe `--aspect-ratio` explícito.

## v0.7: dispositivos externos e experiência de sala

`Scanner` consulta volumes montados a cada dois segundos e acompanha caminhos em `/run/media`,
`/media` e `/mnt`. A conexão de um volume emite um aviso para o usuário decidir se deseja
pesquisar jogos; a desconexão apenas atualiza o estado visual e não remove registros do SQLite.

A biblioteca usa um `ListView` horizontal com snap, foco estrito e ativação por Enter/Espaço,
permitindo navegação por teclado e controles que mapeiam essas teclas. O script
`scripts/setup-dev-environment.sh` cria ou reutiliza um container Toolbox/Distrobox com Qt 6.5+,
CMake e Ninja e executa o build dentro dele.


## v0.3: Steam manifest discovery and launch

The scanner now reads `appmanifest_*.acf` files in common Steam libraries and attempts to locate extra libraries from `libraryfolders.vdf`. Each Steam candidate stores its install directory and `steam://rungameid/<appid>` launch URI. Import is explicit. Launching a catalog item opens Steam URIs via desktop URL handling; explicitly configured commands are split into program/arguments and started without a shell. This is a first implementation and requires validation against real Bazzite installations.
