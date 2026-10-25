# Z-GAME HUB

Central de jogos Zystem para Bazzite Linux, com foco em desktop e Steam Gaming Mode.

## Desenvolvimento v0.8

Esta linha começa a fechar a descoberta Steam antes de avançar para integrações completas
com Lutris, Heroic e Bottles. O scanner reconhece instalações Steam nativas e Flatpak em
caminhos usuais, percorre bibliotecas adicionais declaradas em `libraryfolders.vdf` e
classifica candidatos como Linux nativo ou Proton de forma heurística (`oslist` do manifesto
ou presença do prefixo `compatdata/<appid>/pfx`). O lançamento continua delegado à Steam
por `steam://rungameid/<appid>`; não são executados comandos reconstruídos pelo hub.

Os metadados de biblioteca, prefixo e estado de instalação aparecem nos candidatos do
scanner, mas ainda não são persistidos no catálogo. A classificação Proton é indicativa e
precisa ser validada em Steam nativa e Flatpak no Bazzite. Esta etapa não equivale a uma
integração v0.8 completa nem substitui os testes em hardware real.

## Desenvolvimento v0.9 — bibliotecas de launchers

O scanner consulta `pga.db` do Lutris em modo somente leitura e tenta relacionar os registros
com YAMLs locais para obter runner, executável, diretório de trabalho e prefixo. O lançamento
Lutris usa `lutris:<slug>` a partir do slug lido do banco. No Heroic, o scanner lê JSONs
`installed.json` sob as árvores de configuração nativa e Flatpak e coleta caminho de instalação,
runner e prefixo quando os campos estão disponíveis. Em Bottles, a integração inicial procura
registros de programas dentro das bottles e só habilita a importação quando localiza o
executável referido.

Esses formatos internos variam entre versões; os leitores são best-effort e precisam de
validação em instalações reais nativas e Flatpak. Nenhum comando de YAML/JSON é executado.
## Versão 1.0.0 — lançamento Heroic e Bottles

O leitor Heroic constrói o protocolo `heroic://launch` com `appName` codificado e runner
permitido (`legendary`, `gog` ou `nile`), iniciando o launcher nativo ou Flatpak com
`--no-gui`. O Bottles é consultado por sua CLI oficial: os programas registrados são lidos
em JSON, os caminhos Windows são mapeados para o arquivo no prefixo local, e a execução usa
`bottles-cli run -b ... -p ...` (ou o equivalente Flatpak), com cada argumento passado
separadamente. O hub valida o formato do comando, a disponibilidade do launcher e a existência
do executável antes de iniciar; não usa shell.

Os formatos de bibliotecas e handlers de protocolo ainda precisam de testes em Bazzite real.
A versão 1.0.0 identifica o código desta entrega, mas não certifica estabilidade em todas as
distribuições, versões de Heroic/Bottles ou instalações Flatpak.

## Estado desta entrega

Incluído:
- Aplicação Qt 6/QML com tema escuro, turquesa e navegação inicial.
- Seletor gráfico Qt para adicionar múltiplos executáveis/ROMs ou uma pasta sem digitar caminhos.
- Conversão segura de URLs `file://` para caminhos locais antes da gravação no catálogo.
- Edição e exclusão protegida de registros manuais, sem apagar o arquivo original.
- Filtro rápido por plataforma combinado com a pesquisa da biblioteca.
- Configuração persistente de RetroArch ou outro emulador por plataforma.
- Argumentos seguros com os marcadores `{ROM}` e `{CORE}`, sem uso de shell.
- Sugestão de RetroArch via Flatpak quando o executável nativo não está disponível.
- Suporte a executáveis instalados em caminhos com espaços e comandos `flatpak run`.
- Descoberta de cores `*_libretro.so` em diretórios nativos e do RetroArch Flatpak.
- Sugestão de core por plataforma, sempre editável pelo usuário.
- Presets de execução: personalizado, tela cheia, janela, 4:3 e 16:9.
- Configuração persistente de proporção de tela para cada plataforma.
- Leitura de bibliotecas Lutris, Heroic e Bottles e lançamento seguro pelos launchers (v1.0.0, requer validação em Bazzite).
- Jogos Steam continuam usando `steam://rungameid`, permitindo que a própria Steam escolha Proton quando necessário.
- Fundo hero e fallback visual de cards incluídos como recursos Qt otimizados.
- Componente reutilizável `ZButton` com estados de foco, hover, pressionado e destaque para gamepad.
- Monitoramento automático de volumes externos em `/run/media`, `/media` e `/mnt`, com confirmação antes da varredura.
- Carrossel horizontal com foco, snap, setas, Enter/Espaço e navegação por controle.
- Script `scripts/setup-dev-environment.sh` para criar Toolbox/Distrobox com Qt 6.5+, CMake e Ninja.
- Catálogo local SQLite.
- Assistente de cadastro manual.
- Scanner de diretórios comuns, instalações Steam conhecidas, pastas de emulação e volumes montados.
- Lista de candidatos com título, caminho e tentativa de classificação de plataforma; o usuário escolhe importar ou ignorar cada item.
- Leitura inicial dos manifestos `appmanifest_*.acf` da Steam, com descoberta de bibliotecas adicionais via `libraryfolders.vdf`.
- Importação de entradas Steam com o protocolo `steam://rungameid/APPID`.
- Botão “Iniciar” na biblioteca: abre protocolos Steam ou executa comandos configurados sem passar por shell.
- Limite de segurança de 1.500 candidatos por varredura para evitar listas sem controle.
- Atalhos para build e instalação por usuário.
- Links de descoberta para fontes externas, incluindo MyAbandonware (planejado para a tela Loja).

Ainda não implementado:
- Validação real do lançamento Heroic/Bottles e dos formatos alternativos das bibliotecas em múltiplas versões/distribuições.
- Monitoramento de eventos UDisks2/D-Bus em tempo real; por enquanto, é necessário iniciar a busca manualmente depois de conectar uma unidade.
- Capas e metadados; o seletor gráfico inicial importa caminhos, mas ainda não baixa imagens nem preenche metadados automaticamente.
- Persistência dos metadados de runner/prefixo no catálogo ainda está pendente.
- Loja integrada e gerenciamento de downloads.
- Empacotamento Flatpak/AppImage validado.

## Dependências de desenvolvimento

- CMake 3.21+
- Ninja
- Compilador C++20
- Qt 6.5+ com Core, Gui, Qml, Quick e Sql
- Driver SQLite para Qt

No Bazzite, prefira desenvolver dentro de um ambiente de desenvolvimento apropriado (por exemplo, toolbox/distrobox ou SDK de Flatpak) em vez de modificar a imagem base.

## Compilar

```bash
chmod +x scripts/*.sh
./scripts/build.sh
```

Para compilar em modo Release, gerar o executável e criar um pacote `.tar.gz` para a
arquitetura da máquina de build:

```bash
./scripts/build-release.sh
```

O executável fica em `build/release/z-game-hub` e o pacote em
`dist/z-game-hub-1.0.0-linux-<arquitetura>.tar.gz`. O Qt não é incorporado: o sistema de
destino precisa fornecer Qt 6.5+ com Core, Gui, Qml, Quick e Sql, além do driver SQLite Qt.

Para preparar o ambiente no Bazzite sem modificar a imagem base:

```bash
./scripts/setup-dev-environment.sh auto build
```

Para abrir um shell de desenvolvimento no container:

```bash
./scripts/setup-dev-environment.sh auto shell
```

Na imagem base imutável do Bazzite, instale CMake, Ninja e o SDK Qt 6.5+ dentro de um
Toolbox/Distrobox ou SDK de Flatpak. O projeto não exige e não faz alterações na imagem base.

## Instalar no usuário

```bash
./scripts/install-user.sh
```

A instalação pressupõe que os recursos Qt necessários estejam disponíveis no runtime. O script instala apenas o executável e o arquivo desktop; não modifica a imagem base do Bazzite.

## Desinstalar

```bash
./scripts/uninstall-user.sh
```

O banco de dados é armazenado no diretório de dados do aplicativo e não é apagado pela desinstalação.

## Fontes planejadas para a área Loja e Descoberta

- itch.io: https://itch.io/games
- GOG: https://www.gog.com/
- Internet Archive Software: https://archive.org/details/software
- GitHub Open Source Games: https://github.com/topics/open-source-game
- My Abandonware: https://www.myabandonware.com/

My Abandonware será apresentado como catálogo externo com abertura da página original. O status de abandonware não garante que todos os jogos estejam livres de direitos autorais; verificar a disponibilidade legal e a segurança de cada download é responsabilidade do usuário. O hub não baixa automaticamente arquivos protegidos nem executa conteúdo baixado sem confirmação.

## Segurança e limitações do lançamento

- O botão “Iniciar” abre o protocolo Steam dos jogos importados dos manifestos ou executa o comando que foi cadastrado explicitamente pelo usuário.
- Comandos não são executados por shell. Ainda assim, cadastre apenas executáveis e argumentos confiáveis; o programa não valida a procedência de um binário.
- Jogos adicionados por scanner sem comando de lançamento exibem uma mensagem para configurar o método de execução. Encontrar uma ROM não significa que o emulador esteja configurado.
- O parser VDF é intencionalmente simples e precisa de testes com diferentes versões/instalações da Steam.
- A busca de diretórios ainda é síncrona; unidades grandes podem demorar e o limite de candidatos evita uma lista ilimitada.

Não execute comandos recebidos de metadados ou páginas web. O hub não baixa nem executa conteúdo externo automaticamente.
