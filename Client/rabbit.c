#class auto

#use "i2c_devices.lib"

//network configuration - this one includes DHCP and cannot be changed in runtime
#define TCPCONFIG 3

//server and port
#define SERVER "co2.partycon.net"
#define PORT 80

//#use "net.lib"
#use "dcrtcp.lib"

//rabit's id
#define MY_ID "rabbit1"

//From left to right, the first one only has digit 2
const char maxAddresses[6] = {0xA0, 0xA2, 0xAC, 0xAE, 0xA8, 0xAA};
const char tempSensorAddress = 0x90;

const char testRegister = 0x07;
const char configRegister = 0x4;
const char intensity1Register = 0x01;
const char intensity2Register = 0x02;

const char CO2_WR_ADDRESS = 0x2A;
const char CO2_RD_ADDRESS = 0x2B;
const char CO2_FUNCTION_CODE = 0x04;
char co2Data0, co2Data1, co2Data2, co2Data3;

//defaults
const int defaultIntensity = 15;
const int defaultColor = 1; //orange

//0=green, 1=orange, 2=red
int digitColors[11];

char data, temp, countPerC, countRemain;
char output[2048];
int i, j, address, color, waitInt, waitCount, roundI, roundJ, gas2;
unsigned char readArray[2];
double temperature, temperatureRead, temperature2, temperatureResult;
char colorString[1];
char intensityString[2];
char string[11];
char toWriteString[11];
int intensity, co2;

void wait(){
	for(waitInt = 0; waitInt < 20000; waitInt++){}
}

void waitx(int times){
	for(waitCount = 0; waitCount < times; waitCount++){
		for(waitInt = 0; waitInt < 20000; waitInt++){}
	}
}

void runTest(){
	data = 1;
	for(i = 0; i < sizeof(maxAddresses); i++){
		I2CWrite(maxAddresses[i], testRegister, &data, 1);
	}
	
	for(i = 0; i < 20; i++) wait();

	data = 0;
	for(i = 0; i < sizeof(maxAddresses); i++){
		I2CWrite(maxAddresses[i], testRegister, &data, 1);
	}
}

void wakeup(){
	data = 1;
	for(i = 0; i < sizeof(maxAddresses); i++){
		I2CWrite(maxAddresses[i], configRegister, &data, 1);
	}

	//wakeup for temperature sensor
	//puts it in one-shot mode
	I2CRead(tempSensorAddress, 0xAC, &data, 1);
	data = data | (char) 1;
	I2CWrite(tempSensorAddress, 0xAC, &data, 1);

	wait();
}

double readTemperature(){

	temperature = 0;
	for(i=0; i < 1; i++){

		//starts temperature conversion
		data = 0;
		I2CWrite(tempSensorAddress, 0xEE, &data, 0);

		//waits until DONE bit in config register is set to 1
		while(data<(char)128){
			I2CRead(tempSensorAddress, 0xAC, &data, 1);
		}

		I2CRead(tempSensorAddress, 0xAA, readArray, 2);
		temperatureRead = readArray[0];

		if(readArray[1] == 128) temperatureRead += 0.5;

		I2CRead(tempSensorAddress, 0xA9, &countPerC, 1);
		I2CRead(tempSensorAddress, 0xA8, &countRemain, 1);

		//if(i==0) printf("temperature:%f temperatureRead:%f countPerC:%d countRemain:%d\n", temperature, temperatureRead, countPerC, countRemain);
		//else printf("temperature:%f temperatureRead:%f countPerC:%d countRemain:%d\n", temperature/i, temperatureRead, countPerC, countRemain);

		temperature =  temperature + temperatureRead - 0.25 + (double)((double)(countPerC - countRemain) / (double)countPerC);
		//printf("temperature:%f temperatureRead:%f countPerC:%d countRemain:%d\n", temperature, temperatureRead, countPerC, countRemain);
		//wait();
	}
	
	temperature = temperature/1;
	return temperature;
}

//sample method that prints value from co2 sensor
int readCo2(){
	while(1){
		i2c_start_tx();
		i2c_write_char(CO2_WR_ADDRESS);
		i2c_write_char(CO2_FUNCTION_CODE);
		i2c_write_char(0x13);
		i2c_write_char(0x8B);
		i2c_write_char(0);
		i2c_write_char(1);
		
		i2c_stop_tx();
		i2c_start_tx();

		i2c_write_char(CO2_RD_ADDRESS);
	
		i2c_read_char(&co2Data0);
		i2c_send_ack();
		i2c_read_char(&co2Data1);
		i2c_send_ack();
		i2c_read_char(&co2Data2);
		i2c_send_ack();
		i2c_read_char(&co2Data3);
		i2c_send_nak();
		i2c_stop_tx();
		if(co2Data0 != 0){
			break;
		}
	}
	/*printf("%d, %d, %d, %d: ", co2Data0, co2Data1, co2Data2, co2Data3);
	result = 256*co2Data2 + co2Data3;
	printf("%d\n", result);
	return result;*/
	return 256*co2Data2 + co2Data3;
}

//0=green, 1=orange, 2=red
void setAllDigitsColor(int color){
	for(i = 0; i < 11; i++){
		digitColors[i] = color;
	}
}


//sets whole display's intensity/brightnes
//intensity must be from 0 to 15
void setIntensity(char intensity){

	//for security reasons - ensures that intensity is in valid range
	if(intensity < 0) intensity = 0;
	if(intensity > 15) intensity = 15;

	data = intensity * 16 + intensity;

	for(i = 0; i < sizeof(maxAddresses); i++){
		I2CWrite(maxAddresses[i], intensity1Register, &data, 1);
		I2CWrite(maxAddresses[i], intensity2Register, &data, 1);
	}

}

void cleanup(){
	for(i = 0; i < sizeof(maxAddresses); i++){
		I2CWrite(maxAddresses[i], 0x60, " ", 1);
		I2CWrite(maxAddresses[i], 0x61, " ", 1);
		I2CWrite(maxAddresses[i], 0x62, " ", 1);
		I2CWrite(maxAddresses[i], 0x63, " ", 1);
	}
}

void initialize(){
	i2c_init();
	wakeup();
	cleanup();
	setIntensity(defaultIntensity);
	setAllDigitsColor(defaultColor);
	runTest();
	wait();
}


//void to render a ceretain char on a ceretain digit
void renderChar(char character, int digit){
	if(digit==1 || digit==2) address = maxAddresses[1];
	if(digit==3 || digit==4) address = maxAddresses[2];
	if(digit==5 || digit==6) address = maxAddresses[3];
	if(digit==7 || digit==8) address = maxAddresses[4];
	if(digit==9 || digit==10) address = maxAddresses[5];

	color = digitColors[digit];

	if(digit==0){
		if(color == 0){
			I2CWrite(maxAddresses[0], 0x62, " ", 1);
			I2CWrite(maxAddresses[0], 0x63, &character, 1);
		}else if(color == 1){
			I2CWrite(maxAddresses[0], 0x62, &character, 1);
			I2CWrite(maxAddresses[0], 0x63, &character, 1);
		}else if(color == 2){
			I2CWrite(maxAddresses[0], 0x62, &character, 1);
			I2CWrite(maxAddresses[0], 0x63, " ", 1);
		}
	}else{
		digit = digit % 2;
		if(digit==1){
			//if digit is the first one of the two, controlled by 1 maxim

			if(color == 0){
				I2CWrite(address, 0x60, " ", 1);
				I2CWrite(address, 0x61, &character, 1);
			}else if(color == 1){
				I2CWrite(address, 0x60, &character, 1);
				I2CWrite(address, 0x61, &character, 1);
			}else if(color == 2){
				I2CWrite(address, 0x60, &character, 1);
				I2CWrite(address, 0x61, " ", 1);
			}

		}else if(digit == 0){
			//if digit is the second one of the two, controlled by 1 maxim

			if(color == 0){
				I2CWrite(address, 0x62, " ", 1);
				I2CWrite(address, 0x63, &character, 1);
			}else if(color == 1){
				I2CWrite(address, 0x62, &character, 1);
				I2CWrite(address, 0x63, &character, 1);
			}else if(color == 2){
				I2CWrite(address, 0x62, &character, 1);
				I2CWrite(address, 0x63, " ", 1);
			}
		}
	}

}

void renderString(char string[], int stringColor){
	setAllDigitsColor(stringColor);

	j = 0;
	for(i = 0; i < 11; i++){
		if(j < strlen(string)) {
			if(string[j] == (char) 194) {
				renderChar((char) 27, i);
				j=j+2;
			}else if(string[j] == (char)38){
				renderChar((char) 27, i);
				j++;
			}else {
				renderChar(string[j], i);
				j++;
			}
		}else {
			renderChar(" "[0], i);
			j++;
		}
	}
}

void runWithServerProtocol(){
	int status;
	tcp_Socket socket;
	char buffer[16];
	longword ip;

	renderString("Init", 0);
	printf("Init\n");

	sock_init();
	while (ifpending(IF_DEFAULT) == IF_COMING_UP) {
		tcp_tick(NULL);
	}

	if( 0L == (ip = resolve(SERVER)) ) {
		renderString("DNS error", 2);
		printf("DNS error\n");
		waitx(10);
		return;
	}else{
		renderString("Resolved", 0);
		printf("Resolved\n");
	}

	renderString("Connecting", 0);
	printf("Connecting\n");

	tcp_open(&socket,0,ip,PORT,NULL);
	while(!sock_established(&socket) && sock_bytesready(&socket)==-1) {
		tcp_tick(NULL);
	}

	renderString("Connected", 0);
	printf("Connected\n");
	sock_mode(&socket,TCP_MODE_ASCII);

	wait();

	sock_puts(&socket, MY_ID);
	renderString(MY_ID, 1);

	while(tcp_tick(&socket)) {
		if((sock_bytesready(&socket) != -1) || (sock_bytesready(&socket) > 0)){
			sock_gets(&socket, buffer, 16);

			printf("recieved: %s\n", buffer);
		
			if(strlen(buffer) > 0){
				if(buffer[0] == (char) 116){
					printf("temperature pending\n");
					temperature2 = readTemperature();
					sprintf(output, "%f", temperature2);
					sock_puts(&socket, output);
					printf("temperature: %s\n", output);

				}else if(buffer[0] == (char) 103){
					printf("gas pending\n");
					co2 = readCo2();
					sprintf(output, "%d", co2);
					sock_puts(&socket, output);
					printf("gas: %s\n", output);
				}else{
					colorString[0] = buffer[0];
					//intensityString[0] = buffer[1];
					//intensityString[1] = buffer[2];

					color = atoi(colorString);

					//clear
					for(i=0; i < 11; i++){
						string[i] = 32;
					}

					for(i = 1;  i < strlen(buffer); i++){
						if(i < 11){
							string[i-1] = buffer[i];
						}
					}

					//fill all remaining chars with space
					/*for(; i < strlen(string); i++){
						string[i] = " "[0];
					}*/

					//setIntensity(intensity);
					renderString(string, color);
					sock_puts(&socket, "1");
				}
			}

		}
	}
}

void main(){
	initialize();
	while(1){
		runWithServerProtocol();
		
		/*co2 = readCo2();
		printf("co2: %d\n", co2);
		snprintf(toWriteString, sizeof(toWriteString), "%d ppm", co2);
		renderString(toWriteString, 0);
		waitx(50);

		temperature2 = readTemperature();
		printf("temp: %f\n", temperature2);
		snprintf(toWriteString, sizeof(toWriteString), "%.1f °C", temperature2);
		renderString(toWriteString, 2);
		waitx(50);*/
	}
}
