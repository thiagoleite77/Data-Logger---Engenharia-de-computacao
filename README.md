# 📊 Data Logger Ambiental

> Projeto desenvolvido para a **N2 da disciplina de Sistemas Embarcados**, do curso de **Engenharia de Computação**.
> 
> <img width="856" height="726" alt="Wokiwi" src="https://github.com/user-attachments/assets/33e38bf9-3895-4845-b5ab-b4e081da0b0d" />


# 📖 Sobre o Projeto

O **Data Logger Ambiental** é um sistema embarcado desenvolvido para realizar o monitoramento contínuo de condições ambientais em ambientes controlados.

O equipamento foi projetado utilizando o **ATmega328P (Arduino Uno)** como unidade de processamento e é capaz de medir **temperatura**, **umidade relativa do ar** e **luminosidade**, apresentando as informações em tempo real através de uma Interface Homem-Máquina (IHM) baseada em um display LCD.

Além da visualização instantânea das medições, o sistema registra os dados juntamente com data e hora utilizando um módulo de Relógio de Tempo Real (RTC), permitindo o armazenamento histórico das informações na memória EEPROM do microcontrolador.

O projeto também incorpora mecanismos de alerta visual e sonoro para indicar quando os parâmetros monitorados ultrapassam os limites estabelecidos, tornando-o uma solução simples, intuitiva e eficiente para aplicações de monitoramento ambiental.



A carcaça foi projetada no AutoDesk Inventor, afim de garantir as medições corretas, para impressão 3D com material polimérico PLA.

<img width="1917" height="1026" alt="MODELAGEM 3D" src="https://github.com/user-attachments/assets/52fd4abb-dbfd-424a-b6f1-c033deebfa5d" />



---

#  Objetivos

O projeto tem como objetivo desenvolver um dispositivo embarcado capaz de:

- Monitorar temperatura, umidade e luminosidade;
- Exibir as informações em tempo real ao usuário;
- Registrar medições com data e hora;
- Armazenar o histórico das leituras na EEPROM;
- Alertar o usuário quando os parâmetros estiverem fora das faixas estabelecidas;
- Fornecer uma Interface Homem-Máquina simples e intuitiva.

---

#  Funcionalidades

- Monitoramento de temperatura ambiente;
- Monitoramento da umidade relativa do ar;
- Monitoramento da luminosidade do ambiente;
- Registro das medições com timestamp utilizando RTC;
- Armazenamento de dados na EEPROM;
- Interface de navegação através de três botões;
- Display LCD 16x2 para exibição das informações;
- LEDs indicadores de status;
- Alertas sonoros através de buzzer;
- Navegação por menus para consulta das informações.

---

#  Interface Homem-Máquina (IHM)
<img width="966" height="1629" alt="DataLogger TAJI" src="https://github.com/user-attachments/assets/38c114b8-26e8-41fe-b99e-655aad3b7f95" />


O equipamento foi projetado para oferecer uma interação simples ao usuário.

A interface é composta por:

- Display LCD 16x2 para visualização das informações;
- Três botões para navegação entre menus e confirmação de opções;
- LEDs indicadores para informar rapidamente o estado do sistema;
- Buzzer responsável pelos alertas sonoros em situações críticas.

Essa abordagem permite que qualquer operador consiga utilizar o equipamento de forma intuitiva, sem necessidade de treinamento avançado.

---

# 🚦 Sistema de Alertas

O Data Logger possui indicadores visuais e sonoros para facilitar a identificação do estado de operação.

### 🟢 LED Verde

Indica funcionamento normal.

Todos os parâmetros monitorados encontram-se dentro das faixas especificadas.

---

### 🟡 LED Amarelo

Indica condição de atenção.

Utilizado para sinalizar falhas de leitura, problemas em sensores ou necessidade de verificação do equipamento.

---

### 🔴 LED Vermelho

Indica situação de alerta.

É acionado quando qualquer variável monitorada ultrapassa os limites definidos para operação.

---

###  Buzzer

Complementa os alertas visuais emitindo sinais sonoros quando ocorre alguma condição crítica.

---

# 📏 Faixas de Operação

| Variável | Faixa de Operação |
|-----------|-------------------|
| Temperatura | **15°C < T < 25°C** |
| Umidade Relativa | **30% < U < 50%** |
| Luminosidade | **0% < L < 30%** |

---

#  Componentes Utilizados

- Arduino Uno R3 (ATmega328P)
- Display LCD 16x2 I2C
- Sensor DHT11
- Sensor LDR
- RTC (Real Time Clock)
- EEPROM Interna
- LEDs indicadores
- Buzzer
- Botões de navegação
- Protoboard
- Resistores
- Jumpers
- Alimentação por bateria de 9V

---

#  Aplicação

O projeto foi concebido para monitoramento ambiental em ambientes controlados, podendo ser utilizado como base para aplicações em:

- Laboratórios;
- Almoxarifados;
- Estoques;
- Salas climatizadas;
- Ambientes industriais;
- Controle de armazenamento;
- Projetos acadêmicos de Sistemas Embarcados.

---

# 📚 Tecnologias Utilizadas

- C++
- Arduino IDE
- Arduino Uno
- ATmega328P
- Comunicação I²C
- RTC
- EEPROM
- Git
- GitHub
- AutoDesk Inventor
- impressão 3D

---

# 👨‍💻 Equipe

- **Thiago Leite de Souza**
- **Andreo Sampaio** 
- **João Paulo**
- **Ítalo Silva**

---

# 🎓 Disciplina

**Sistemas Embarcados**

Curso de **Engenharia de Computação**

Projeto desenvolvido como avaliação **N2**.

---

# 📄 Licença

Este projeto possui finalidade exclusivamente acadêmica e foi desenvolvido para a disciplina de **Sistemas Embarcados**.
