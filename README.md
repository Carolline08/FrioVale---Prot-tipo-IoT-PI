FRIOVALE — SISTEMA IoT PARA MONITORAMENTO DA CADEIA DO FRIO

**Integrantes:**
Abigail Maria 
Carolline Barbosa 
Kallyne Melo
Marcelly Arcanjo 
Maria Cecília 
Pedro Henrique Barros

**Introdução**

O FrioVale é um protótipo IoT desenvolvido para monitorar temperatura e umidade em ambientes utilizados no armazenamento e transporte de frutas no Vale do São Francisco.

O sistema utiliza ESP32, sensor DHT11, conexão Wi-Fi e ThingSpeak para realizar a coleta e visualização dos dados.

**Problema**

Durante o armazenamento e transporte, variações de temperatura e umidade podem prejudicar a qualidade das frutas.

O monitoramento manual pode ser limitado, dificultando a identificação rápida de condições inadequadas.

**Objetivo**

Desenvolver um dispositivo de baixo custo capaz de medir temperatura e umidade e enviar essas informações para uma plataforma de monitoramento online.

**Aplicação do Projeto**
O dispositivo pode ser instalado em:

• Câmaras frias
• Packing houses
• Contêineres refrigerados
• Veículos de transporte refrigerado
• Áreas de armazenamento pós-colheita

**Funcionamento do Protótipo**
O DHT11 realiza a leitura da temperatura e da umidade.

O ESP32 processa os valores recebidos e utiliza a rede Wi-Fi para enviar os dados ao ThingSpeak.

Os dados ficam disponíveis em gráficos para acompanhamento da condição do ambiente.

**Principais Componentes**
• ESP32
• Sensor DHT11
• Cabos e protoboard
• Fonte de alimentação
• LEDs de indicação, quando utilizados
• Caixa reutilizada para proteção do circuito

**Canal de Telemetria**
O projeto utiliza o ThingSpeak para receber e apresentar os dados coletados.


**Registro Fotográfico**
Deve ser apresentado:

• Foto da montagem interna, mostrando o ESP32, DHT11, conexões e circuito.
• Foto do protótipo fechado dentro da caixa reutilizada.
• Foto destacando as aberturas de ventilação e a organização dos componentes.

**Código-Fonte**
O código deve ser organizado e comentado, facilitando a compreensão e manutenção do projeto.

As configurações principais devem incluir:

• Conexão Wi-Fi
• Leitura do DHT11
• Processamento das informações
• Envio dos dados para o ThingSpeak
• Intervalo entre as leituras
• Indicação de funcionamento por LED, quando utilizada

**Segurança das Credenciais**
As informações da rede Wi-Fi e as chaves da API não devem ser colocadas diretamente no código público.

Recomenda-se utilizar um arquivo separado, como credentials.h, e adicionar esse arquivo ao .gitignore.

Exemplo:

WIFI_SSID = "SUA_REDE"

WIFI_PASSWORD = "SUA_SENHA"

THINGSPEAK_API_KEY = "SUA_CHAVE"

Configuração do ESP32

Para utilizar o projeto, é necessário instalar o suporte da placa ESP32 na Arduino IDE, configurar a placa correta e instalar as bibliotecas utilizadas pelo código.

Também devem ser configurados corretamente:

• Pino do DHT11
• Alimentação do sensor
• Rede Wi-Fi
• Chave do ThingSpeak
• Intervalo de leitura

**Memorial Descritivo**
O sistema realiza a leitura periódica da temperatura e umidade, processa os valores no ESP32 e envia as informações pela rede Wi-Fi para o ThingSpeak.

O objetivo é permitir o acompanhamento das condições ambientais de forma simples e acessível.

Limitações do DHT11

O DHT11 possui baixo custo, porém apresenta limitações de precisão, resolução e velocidade de resposta.

Por isso, ele é adequado para demonstração e prototipagem, mas pode não ser suficiente para aplicações agrícolas de exportação que exigem maior precisão e controle.

Também é necessário considerar problemas causados por condensação e alta umidade.

Limitações do Wi-Fi

O Wi-Fi depende da existência de uma rede disponível.

Em câmaras frias, galpões, veículos e durante o transporte, o sinal pode ser instável ou inexistente.

O protótipo também não utiliza atualmente tecnologias de baixo consumo, como Deep Sleep ou LoRaWAN.

Limitações da Caixa

A caixa reutilizada atende ao objetivo de prototipagem, mas não possui necessariamente proteção adequada contra poeira, água e umidade.

As aberturas de ventilação são importantes para permitir que o sensor tenha contato com o ambiente, porém podem reduzir a proteção do circuito.

Para uma versão comercial, seria necessário utilizar uma caixa com proteção adequada, como uma classificação IP compatível com o ambiente.

Resultados Esperados

Espera-se que o protótipo consiga:

• Medir temperatura e umidade.
• Enviar os dados pela internet.
• Apresentar os valores no ThingSpeak.
• Demonstrar o funcionamento de um sistema IoT aplicado à cadeia do frio.
• Servir como base para uma versão mais robusta do FrioVale.

**Trabalhos Futuros**
Entre as melhorias previstas estão:

• Utilização de sensores mais precisos.
• Comunicação 4G/LTE.
• Armazenamento dos dados sem internet.
• Alertas para temperatura fora dos limites.
• GPS para rastreamento.
• Banco de dados próprio.
• Dashboard web ou aplicativo.
• Caixa com maior proteção contra umidade e poeira.
• Utilização de tecnologias de baixo consumo.

**Conclusão**
O FrioVale demonstra a aplicação da Internet das Coisas no monitoramento da cadeia do frio.

O protótipo utiliza ESP32, DHT11, Wi-Fi e ThingSpeak para coletar, transmitir e visualizar dados de temperatura e umidade.

Apesar das limitações do sensor, da comunicação Wi-Fi e da caixa reutilizada, o projeto apresenta uma base funcional para futuras melhorias e para uma solução mais adequada às necessidades do transporte e armazenamento de frutas no Vale do São Francisco.

FrioVale — Tecnologia para monitorar. Dados para decidir. Qualidade para preservar.
