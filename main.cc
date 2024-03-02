#include <iostream>
#include <fstream>
#include <cstdio>
#include <cmath>
#include <random>
#include <vector>
#include <bitset>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <SDL.h>
#include <SFML/Graphics.hpp>

#define SLEEP_TIME 2500

typedef unsigned char  byte;
typedef unsigned short word;

static byte RAM[0x1000] =
{
    // System fonts
    0xf0,0x90,0x90,0x90,0xf0, // 0
    0x20,0x60,0x20,0x20,0x70, // 1
    0xf0,0x10,0xf0,0x80,0xf0, // 2
    0xf0,0x10,0xf0,0x10,0xf0, // 3
    0x90,0x90,0xf0,0x10,0x10, // 4
    0xf0,0x80,0xf0,0x10,0xf0, // 5
    0xf0,0x80,0xf0,0x90,0xf0, // 6
    0xf0,0x10,0x20,0x40,0x40, // 7
    0xf0,0x90,0xf0,0x90,0xf0, // 8
    0xf0,0x90,0xf0,0x10,0xf0, // 9
    0xf0,0x90,0xf0,0x90,0x90, // A
    0xe0,0x90,0xe0,0x90,0xe0, // B
    0xf0,0x80,0x80,0x80,0xf0, // C
    0xe0,0x90,0x90,0x90,0xe0, // D
    0xf0,0x80,0xf0,0x80,0xf0, // E
    0xf0,0x80,0xf0,0x80,0x80  // F
};

static byte VRAM[0x800] = {0};
static byte V[16];
static word I;
static word Stack[16];
static byte Keys[16];
static word SP = 0;
static byte *ProgramStart = &RAM[0x200];
static word PC = 0x200;
static byte DT = 0; // Delay timer
static byte ST = 0; // Sound timer
static bool WaitingKey = false;

static unsigned long long Counter = 0;
static sf::RenderWindow MainWindow(sf::VideoMode(640, 320), "chip8");
std::vector<std::string> Themes = {"h4x0r", "gb", "coolblue", "takorii"};

struct Audio {
    SDL_AudioSpec Spec {};
    SDL_AudioDeviceID Device;
    unsigned Pitch = 0, WaveCounter = 0;
    
    Audio()
    {
        Spec.freq=96000;
        Spec.format=AUDIO_F32SYS;
        Spec.channels=1;
        Spec.samples=4096;
        Spec.userdata=this;
        Spec.callback=[](void *Param, Uint8 *Stream, int Len)
                      {
                          ((Audio *)Param)->Callback((float *)Stream,Len/sizeof(float));
                      };
        Device = SDL_OpenAudioDevice(nullptr, 0, &Spec, &Spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
        SDL_PauseAudioDevice(Device, 0);
    }
    
    ~Audio()
    {
        SDL_CloseAudioDevice(Device);
    }
    
    void Callback(float *Target, int NumSamples)
    {
        for (int Position = 0; Position < NumSamples; ++Position) {
            float Sample = ((WaveCounter / Spec.freq) % 2 == 0) ? (float)-0.05 : (float)0.05;
            WaveCounter+=Pitch*2;
            Target[Position]=Sample;
        }
    }
};

void
LoadROM(const char *Filename)
{
    FILE *FilePtr = fopen(Filename, "rb");
    unsigned int FileSize;
    
    if (FilePtr==NULL) {
        printf("Error: file '%s' was not found\n", Filename);
        exit(1);
    }
    
    fseek(FilePtr, 0, SEEK_END);
    FileSize = ftell(FilePtr);
    fseek(FilePtr, 0, SEEK_SET);
    
    fread((void *)ProgramStart, 1, FileSize, FilePtr);
    fclose(FilePtr);
}

int
PlaySound(void *Ptr)
{
    long Delay = (ST/60)*1000000;
    ST=0;
    Audio Beeper;
    Beeper.Pitch = 200;
    usleep(Delay);
    return 0;
}

void
RunIns(word Opcode)
{
    word Address;
    byte Nibble, X, Y, KK;
    
    if (DT!=0) {
        std::cout << "waiting dt\n";
        while (DT!=0) {
            DT--;
            usleep(16666);
        }
        std::cout << "dt ok\n";
    }
    
    if (ST!=0) {
        SDL_CreateThread(PlaySound, "AudioThread", (void *)nullptr);
    }
    
    switch(Opcode&0xf000)
    {
        case 0x0000:
            switch (Opcode&0x00ff)
            {
                case 0x00e0:
                    std::fill(VRAM, VRAM+sizeof(VRAM), 0);
                    printf("CLS");
                    break;
                
                case 0x00ee:
                    PC=Stack[SP];
                    SP--;
                    printf("RET");
                    break;
                
                case 0x0000:
                    Address = Opcode&0x0fff;
                    PC=Address;
                    PC-=2;
                    printf("SYS $%%%03x", Address);
                    break;
            }
            break;
        
        case 0x1000:
            Address=Opcode&0x0fff;
            PC=Address;
            PC-=2;
            printf("JP $%%%03x", Address);
            break;
            
        case 0x2000:
            Address=Opcode&0x0fff;
            SP++;
            Stack[SP] = PC;
            PC=Address;
            PC-=2;
            
            printf("CALL $%%%03x", Address);
            break;

        case 0x3000:
            X=(Opcode&0x0f00)>>8;
            KK=Opcode&0x00ff;
            
            if (V[X] == KK) PC+=2;
            printf("SE V%01x $%02x", X, KK);
            break;

        case 0x4000:
            X=(Opcode&0x0f00)>>8;
            KK=Opcode&0x00ff;
        
            if (V[X]!=KK) PC+=2;
            printf("SNE V%01x $%02x", X, KK);
            break;
        
        case 0x5000:
            X = (Opcode&0x0f00)>>8;
            Y = (Opcode&0x00f0)>>4;
        
            if (V[X]==V[Y]) PC+=2;
            printf("SE V%01x V%01x", X, Y);
            break;

        case 0x6000:
            X =(Opcode&0x0f00)>>8;
            KK=Opcode&0x00ff;
        
            V[X] = KK;
        
            printf("LD V%01x $%02x", X, KK);
            break;
        
        case 0x7000:
            X =(Opcode&0x0f00)>>8;
            KK=Opcode&0x00ff;
        
            V[X]+=KK;
        
            printf("ADD V%01x $%02x", X, KK);
            break;
            
        case 0x8000:
            switch (Opcode&0x000f)
            {
                case 0x0000:
                    X=(Opcode&0x0f00)>>8;
                    Y=(Opcode&0x00f0)>>4;
                
                    V[X]=V[Y];
                
                    printf("LD V%01x V%01x", X, Y);
                    break;
                
                case 0x0001:
                    X=(Opcode&0x0f00)>>8;
                    Y=(Opcode&0x00f0)>>4;
                
                    V[X]=V[X]|V[Y];
                
                    printf("OR V%01x V%01x", X, Y);
                    break;
                    
                case 0x0002:
                    X=(Opcode&0x0f00)>>8;
                    Y=(Opcode&0x00f0)>>4;
                    
                    V[X]=V[X]&V[Y];
                    
                    printf("AND V%01x V%01x", X, Y);
                    break;
                    
                case 0x0003:
                    X=(Opcode&0x0f00)>>8;
                    Y=(Opcode&0x00f0)>>4;
                    
                    V[X]=V[X]^V[Y];
                    
                    printf("XOR V%01x V%01x", X, Y);
                    break;
                
                case 0x0004:
                    X=(Opcode&0x0f00)>>8;
                    Y=(Opcode&0x00f0)>>4;
                    
                    V[X] = (V[X]+V[Y])&0x00ff;
                    (V[X]>255)?V[0xf]=1:V[0xf]=0;
                    
                    printf("ADD V%01x V%01x (VF = carry)", X, Y);
                    break;
                    
                case 0x0005:
                    X=(Opcode&0x0f00)>>8;
                    Y=(Opcode&0x00f0)>>4;
                    
                    V[X]>V[Y] ? V[0xf]=1 : V[0xf]=0;
                    V[X]-=V[Y];
                    
                    printf("SUB V%01x V%01x", X, Y);
                    break;
                    
                case 0x0006:
                    X=(Opcode&0x0f00)>>8;
                    Y=(Opcode&0x00f0)>>4;
                    
                    (V[X]&1)==1 ? V[0xf]=1 : V[0xf]=0;
                    V[X]/=2;
                    
                    printf("SHR V%01x V%01x", X, Y);
                    break;
                
                case 0x0007:
                    X=(Opcode&0x0f00)>>8;
                    Y=(Opcode&0x00f0)>>4;
                    
                    V[Y]>V[X] ? V[0xf]=1 : V[0xf]=0;
                    V[X]=V[Y]-V[X];
                    
                    printf("SUBN V%01x V%01x", X, Y);
                    break;
                
                case 0x000e:
                    X=(Opcode&0x0f00)>>8;
                    Y=(Opcode&0x00f0)>>4;
                    
                    ((X&0xc0)>>6)==1 ? V[0xf]=1 : V[0xf]=0;
                    V[X]*=2;
                    
                    printf("SHL V%01x V%01x", X, Y);
                    break;
            }
            
            break;
        break;
        
        case 0x9000:
            X=(Opcode&0x0f00)>>8;
            Y=(Opcode&0x00f0)>>4;
            
            if (V[X]!=V[Y]) PC+=2;
            
            printf("SNE V%01x V%01x", X, Y);
            break;
        
        case 0xa000:
            Address=Opcode&0x0fff;
        
            I=Address;
        
            printf("LD I %03x", Opcode&0x0fff);
            break;
        
        case 0xb000:
            Address=Opcode&0x0fff;
            
            PC=Address+V[0];
            PC-=2;
        
            printf("JP V0 %03x", Opcode&0x0fff);
            break;
        
        case 0xc000:
            {
            std::random_device rd;
            std::mt19937 rng(rd());
            std::uniform_int_distribution<int> uni(0, 255);
        
            X=(Opcode&0x0f00)>>8;
            KK=Opcode&0x00ff;

            V[X]= (uni(rng))&KK;
        
            printf("RND V%01x %02x", (Opcode&0x0f00)>>8, Opcode&0x00ff);
            }
            break;
        
        case 0xd000:
            X=(Opcode&0x0f00)>>8;
            Y=(Opcode&0x00f0)>>4;
            Nibble=(Opcode&0x000f);
            
            {
                for (unsigned Row=0; Row<Nibble; Row++) {
                    word Pixel = RAM[I+Row];
                    for (unsigned Column=0; Column<8; Column++) {
                        if ((Pixel&(0x80>>Column))!=0) {
                            if (VRAM[(V[X]+Column+((V[Y]+Row)*64))]==1)
                                V[0xf]=1;
                            VRAM[V[X]+Column+((V[Y]+Row)*64)]^=1;
                        }
                    }
                }
            }
            
            printf("DRW V%01x V%01x $%01x", X, Y, Nibble);
            break;
        
        case 0xe000:
            switch (Opcode&0x00ff)
            {
                case 0x009e:
                    X=(Opcode&0x0f00)>>8;
                    if (Keys[V[X]])  PC+=2;
                    printf("SKP V%01x", X);
                    break;
                
                case 0x00a1:
                    X=(Opcode&0x0f00)>>8;
                    if (!Keys[V[X]]) PC+=2;
                    printf("SKNP V%01x", X);
                    break;
            }
        
            break;
        
        case 0xf000:
            X=(Opcode&0x0f00)>>8;
            switch (Opcode&0x00ff)
            {
                case 0x0007:
                    V[X]=DT;
                    printf("LD V%01x DT", X);
                    break;
                
                case 0x000a:
                    X=(Opcode&0x0f00)>>8;
                    printf("LD V%01x K", X);
                    break;
                
                case 0x0015:
                    DT=V[X];
                    printf("LD DT, V%01x", X);
                    break;
                    
                case 0x0018:
                    ST=V[X];
                    printf("LD ST, V%01x", X);
                    break;
                    
                case 0x001e:
                    I+=V[X];
                    printf("ADD I, V%01x", X);
                    break;
                    
                case 0x0029:
                    I=V[X]*5;
                    printf("LD 0x%01x, V%01x", V[X], X);
                    break;
                
                case 0x0033:
                    RAM[I]  =(int)V[X]/100;
                    RAM[I+1]=(int)(V[X]/10)%10;
                    RAM[I+2]=(int)V[X]%10;
                    
                    printf("LD B, V%01x", X);
                    break;
                
                case 0x0055:
                    for (unsigned i=0; i<X+1; i++)
                        RAM[I+i]=V[i];
                    
                    printf("LD [I], V%01x", X);
                    break;
                    
                case 0x0065:
                    for (unsigned i=0; i<X+1; i++)
                        V[i]=RAM[I+i];
                
                    printf("LD V%01x [I]", X);
                    break;
                    
                default:
                    printf("Not implemented.");
                    break;
            }
            break;
        
        default:
            printf("Not implemented.");
            break;
    }
    
    printf("\n\n");
}

void
Cycle()
{
    word Opcode = RAM[PC]<<8|RAM[PC+1];

    Counter++;
    
    printf("I = %04x (%04x)\n", I, RAM[I]);
    
    for (unsigned i=0; i<16; i++) {
        if(i%4==0&i!=0) printf("\n");
        printf("V%01x = 0x%02x; ", i, V[i]);
    }
    
    printf("\n\n");
    
    printf("STACK:\n");
    for (unsigned i=15; i>0; i--)
        printf("%01x - %04x\n", i, Stack[i]);
    
    printf("\n\n");
    
    printf("INPUT:\n");
    for (unsigned i=0; i<16; i++) {
        if(i%4==0&i!=0) printf("\n");
        printf("K%01x = %d; ", i, Keys[i]);
    }
    
    printf("\n\n");
    
    printf("PC: 0x%04x | Opcode: 0x%04x | Stack Pointer: %04x\n\n", PC, Opcode, SP+1);
    RunIns(Opcode);
    if (WaitingKey) return;
    PC+=2;
    printf("--------------------------------------------\n\n");
}

void
SaveRAM()
{
    std::ofstream File("ram.dat");
    File.write((char*)RAM, sizeof(RAM));
    File.close();
}

int
main(int argc, char **argv)
{
    if (argc<2) {
        std::cerr << "usage: chip [rom] [theme number(optional)]\n" <<
                     "------------------------------------------\n" <<
                     "Themes: h4x0r; gb; coolblue; takorii\n";
        exit(1);
    }
    
    LoadROM(argv[1]);
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    std::string ChosenTheme;
    sf::Color BackgroundColor, ForegroundColor;
    
    if (argc>2) ChosenTheme=argv[2]; else ChosenTheme="default";
    
    std::unordered_map<std::string, int> ThemeMap;
    std::unordered_map<int,int> KeyMap {
        {sf::Keyboard::Num1,0x1},{sf::Keyboard::Num2,0x2},
        {sf::Keyboard::Num3,0x3},{sf::Keyboard::Num4,0xc},
        {sf::Keyboard::Q,   0x4},{sf::Keyboard::W,   0x5},
        {sf::Keyboard::E,   0x6},{sf::Keyboard::R,   0xd},
        {sf::Keyboard::A,   0x7},{sf::Keyboard::S,   0x8},
        {sf::Keyboard::D,   0x9},{sf::Keyboard::F,   0xe},
        {sf::Keyboard::Z,   0xa},{sf::Keyboard::X,   0x0},
        {sf::Keyboard::C,   0xb},{sf::Keyboard::V,   0xf}
    };
    
    for (unsigned i=0; i<Themes.size(); i++)
        ThemeMap[Themes[i]]=i+1;
    
    switch(ThemeMap[ChosenTheme])
    {
        // h4x0r
        case 1:
            BackgroundColor = sf::Color::Black;
            ForegroundColor = sf::Color::Green;
            break;
        
        // gb
        case 2:
            BackgroundColor = sf::Color(155, 188, 15);
            ForegroundColor = sf::Color(15,  56,  15);
            break;
        
        // coolblue
        case 3:
            BackgroundColor = sf::Color(97, 134, 169);
            ForegroundColor = sf::Color(33, 41,  70);
            break;
        
        // takorii
        case 4:
            BackgroundColor = sf::Color(255, 107, 153);
            ForegroundColor = sf::Color(204, 204, 204);
            break;
        
        // default (b&w)
        default:
            BackgroundColor = sf::Color::Black;
            ForegroundColor = sf::Color::White;
            break;
    }
    
    while (MainWindow.isOpen()) {
        sf::Event Event;
        while (MainWindow.pollEvent(Event)) {
            std::cout << "entered event loop\n";
            switch (Event.type)
            {
                case sf::Event::Closed:
                    MainWindow.close();
                    break;
                
                case sf::Event::KeyReleased:
                    std::cout<<"released\n";
                    break;
                
                default:
                    break;
            }
                        
        }
        
        MainWindow.clear(sf::Color::Black);
            
        Cycle();

        // Draw VRAM to the screen
        for (unsigned Pixel=0; Pixel<sizeof(VRAM); Pixel++) {
            for (int i=7; i>=0; i--) {
                sf::RectangleShape PixelRect(sf::Vector2f(10, 10));
                PixelRect.setPosition(((Pixel%64)+i)*10, (Pixel/64)*10);
                
                if(((VRAM[Pixel])>>i)&0x1) {
                    PixelRect.setFillColor(ForegroundColor);
                } else {
                    PixelRect.setFillColor(BackgroundColor);
                }
                
                MainWindow.draw(PixelRect);
            }
        }

        MainWindow.display();

		usleep(SLEEP_TIME);
    }
    
    return 0;
}













