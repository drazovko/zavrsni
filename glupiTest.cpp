#include<iostream>
#include<string>
#include<cstring>
#include<fstream>
#include<vector>
#include<Poco/Net/DatagramSocket.h>
#include<Poco/Net/SocketAddress.h>
#include<Poco/Timespan.h>
#include<Poco/Net/Socket.h>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<array>
#include<Poco/ByteOrder.h>

template<typename Elem, size_t kapacitet>
class KruzniSpremnik
{
private:
    std::array<Elem, kapacitet> niz;
    size_t pocetak;             //index početka cirkularnog spremnika
    size_t broj;                //trenutni broj elemenata

    std::mutex brava;
    std::condition_variable daLiJePun;
    std::condition_variable daLiJePrazan;

public:
    KruzniSpremnik() : pocetak{0}, broj{0} { }
    
    void Dodaj(const Elem& elem){
        std::unique_lock<decltype(brava)> zasun(brava);
        //stani ovdje ako više nema mjesta i čekaj obavijest:
        daLiJePun.wait(zasun, [this] () { return broj != kapacitet; });
        //dodaj element na kraj
        niz[DajKraj()] = elem;
        ++broj;

        daLiJePrazan.notify_one();
    }

    Elem Sljedeci(){
        std::unique_lock<decltype(brava)> zasun(brava);
        daLiJePrazan.wait(zasun, [this](){ return broj != 0; });

        //vadi element iz niza:
        Elem rez = niz[pocetak];
        pocetak = ++pocetak % kapacitet;
        --broj;
        //obavijesti nit koja čeka da je došlo do promjene:
        daLiJePun.notify_one();
        return rez;
    }

private:
    //pomoćna funkcija za izračunavanje indeksa kraja niza
    size_t DajKraj(){
        return (pocetak + broj) % kapacitet;
    }
};

using namespace std;
using namespace Poco::Net;

struct IdentifikacijaSocketa{
    uint8_t tipArdese;
    uint32_t IPAdresa;
    uint16_t port;
};

struct PrijemnaPoruka{
    uint8_t tipPoruke;
    uint64_t identifikatorStrujanja;
    IdentifikacijaSocketa adresa1;
    IdentifikacijaSocketa adresa2;
};

KruzniSpremnik<PrijemnaPoruka, 100> cirkularniBafer; //gobalni spremnik
Poco::ByteOrder byteOrderMoj;

//dretva koja prazni spremnik
void Trosilo(int id){
    
    while(true){
        PrijemnaPoruka broj = cirkularniBafer.Sljedeci();
        cout << "Trosilo " << id << " je dohvatilo " 
             << hex << broj.identifikatorStrujanja << dec << endl;
        
    }
    
    
}
//dretva koja puni spremnik
void Punjac(int n){
    static uint64_t brojacPunjenja = 1;
    
    //cirkularniBafer.Dodaj(n);
    cout << "Punjac je napravio " << brojacPunjenja++
         << ".poruku" << endl;
}

enum konfParametri {
        IPadresa, port, relayServeri, nekiNoviParametar
};

enum imePoruke {
    MSG_PING = 1,
    MSG_PONG,
    MSG_PONG_REG_REQ,
    MSG_STREAM_ADVERTISEMENT,
    MSG_STREAM_REGISTERED,
    MSG_IDENTIFIER_NOT_USABLE,
    MSG_FIND_STREAM_SOURCE,
    MSG_STREAM_SOURCE_DATA,
    MSG_STREAM_REMOVE,
    MSG_MULTIMEDIA,
    MSG_REQUEST_STREAMING,
    MSG_FORWARD_PLAYER_READY,
    MSG_PLAYER_READY,
    MSG_SOURCE_READY,
    MSG_REQ_RELAY_LIST,
    MSG_RELAY_LIST,
    MSG_SHUTTING_DOWN,
    MSG_PLEASE_FORWARD,
    MSG_REGISTER_FORWARDING 
};

class UcitavanjeKonfiguracije
{
private:
    string cijeliRed;
    vector<string> sviParametri;
    const string imeUlazneKonfiguracije{"konfiguracija.txt"};
    string onoBitno;
    vector<string>relayPosluzitelji;
    
public:
    UcitavanjeKonfiguracije() {
        ifstream tokPremaKonfiguraciji{imeUlazneKonfiguracije};
        while(getline(tokPremaKonfiguraciji, cijeliRed)){
            if ((!cijeliRed.empty()) && (cijeliRed != "\r")) {
                sviParametri.push_back(cijeliRed);    
            }
        }
     }
    ~UcitavanjeKonfiguracije() { }
    void IspisiSveParametre(){
        cout << "---Svi-parametri-ucitani-iz-konf.txt-----" << endl;
        for(const string& red : sviParametri)
            cout << red << endl;
        cout << "-----------------------------------------" << endl;
    }
    string DajParametar(konfParametri kojiParametar){
        size_t pocetak = 0;
        size_t kraj = 0;
        size_t duzinaStringa = 0;
        size_t pozicija = 0;
        onoBitno.clear();
        switch (kojiParametar)
        {
            case nekiNoviParametar:
                cijeliRed.clear();
                cijeliRed.assign(sviParametri[kojiParametar]);
                pocetak = cijeliRed.find_first_of("\"") + 1;
                kraj = cijeliRed.find_first_of("\"", pocetak+1);
                kraj = kraj - pocetak;
                onoBitno = cijeliRed.substr(pocetak, kraj);
                cout << onoBitno << endl;
                break;
            case IPadresa:
            case port:
            case relayServeri:
                cijeliRed.clear();
                cijeliRed.assign(sviParametri[kojiParametar]);
                do
                {
                    pocetak = cijeliRed.find_first_of("\"", kraj+1) + 1;
                    kraj = cijeliRed.find_first_of("\"", pocetak+1);
                    pozicija = cijeliRed.find_first_of(",", kraj);
                    duzinaStringa = kraj - pocetak;
                    onoBitno.append(cijeliRed.substr(pocetak, duzinaStringa));
                    
                    if (pozicija != string::npos) {
                        onoBitno.append(",");
                    }
                    
                } while (pozicija != string::npos);
                break;
            default:
                cout << "Krivi parametar" << endl;
                onoBitno = "Krivi parametar";
                break;
        }
        return onoBitno;
    }
};


class PorukaMajstor
{
private:
    string pingPoruka;
    
public:
    PorukaMajstor(/* args */) { }
    ~PorukaMajstor() { }
    string Ping(){
        uint8_t tipPoruke = 1;
        pingPoruka.append(to_string(tipPoruke));
        pingPoruka.append("Ovo je ping poruka");
        return pingPoruka;
    }

};


class ProvjeraRelayPosluzitelja
{
private:
    string popisPosluzitelja;
    size_t pocetak = 0;
    size_t kraj = 0;
    size_t duzinaStringa = 0;
    size_t dvotocka = 0;
    size_t duzinaIPadrese = 0;
    size_t duzinaPorta = 0;
    string posluziteljIPadresa;
    string posluziteljPort;
    string poruka;
    char poljeZaPrijem[1024];
    string stringZaPrijem;
    string popisAktivnihRelayPosluzitelja;
public:
    ProvjeraRelayPosluzitelja(string posluzitelji) : popisPosluzitelja(posluzitelji) {  }
    ~ProvjeraRelayPosluzitelja() { }

    void Provjera(DatagramSocket& ds){
        int i = 1;
        popisAktivnihRelayPosluzitelja.clear();
        do
        {
            // u popisu poslužitelja su svi svi relay polužitelji pa se iz tog popisa 
            // vade ip adrese i portovi za svaki pojedinačno i radi se ping pong i od 
            // kojeg se dobije odziv njegovi podaci se upisuju u listu
            kraj = popisPosluzitelja.find_first_of(",", pocetak);
            dvotocka = popisPosluzitelja.find_first_of(":", pocetak);
            duzinaStringa = kraj - pocetak;
            duzinaIPadrese = dvotocka - pocetak;
            duzinaPorta = kraj - dvotocka - 1;
            posluziteljIPadresa.assign(popisPosluzitelja.substr(pocetak, duzinaIPadrese));
            posluziteljPort.assign(popisPosluzitelja.substr(dvotocka+1, duzinaPorta));
            pocetak = kraj + 1;
            
            PorukaMajstor porukaMajstor;
            poruka = porukaMajstor.Ping();
            SocketAddress socAddrRelayPosluzitelja(posluziteljIPadresa, posluziteljPort);
            ds.sendTo(poruka.data(), poruka.size(), socAddrRelayPosluzitelja);
            cout << i++ << ". relay server: " << socAddrRelayPosluzitelja.toString() << endl;
            try
            {
                int n = ds.receiveFrom(poljeZaPrijem, sizeof(poljeZaPrijem), socAddrRelayPosluzitelja);
                stringZaPrijem.assign(poljeZaPrijem);
                stringZaPrijem.pop_back();
                if (n == 19 && (stringZaPrijem == "2Ovo je ping poruka")) {
                    if (!popisAktivnihRelayPosluzitelja.empty()) {
                        popisAktivnihRelayPosluzitelja.append(", ");
                    }
                    popisAktivnihRelayPosluzitelja.append(socAddrRelayPosluzitelja.toString());
                    cout << "Potvrđen relay server: " << socAddrRelayPosluzitelja.toString() << endl;
                }
            }
            catch(const std::exception& e)
            {
                std::cerr << e.what() << '\n';
            }
        } while (kraj != string::npos);
    }

    string& DajPopisAktivnihPosluzitelja(){
        return popisAktivnihRelayPosluzitelja;
    }
};



int main()
{
    const int vrijemeCekanjaUSecReceiveFrom = 1;
    const int vrijemeCekanjaUMiliSecReceiveFrom = 0;
    Poco::Timespan timeSpanZaPrijem;
    timeSpanZaPrijem.assign(vrijemeCekanjaUSecReceiveFrom, vrijemeCekanjaUMiliSecReceiveFrom);
    u_char poljeZaPrijem[1024];

    //1. FAZA INICIJALIZACIJE
        //učitavanje parametara iz konfiguracijske datoteke u objekt citac
    UcitavanjeKonfiguracije citac;
    citac.IspisiSveParametre();
        //priprema posluzitelja za komunikaciju
    SocketAddress sa(citac.DajParametar(IPadresa), citac.DajParametar(port));
    DatagramSocket ds(sa);
    ds.setReceiveTimeout(timeSpanZaPrijem); //podešavanje koliko će dugo server čekati na prijemu
    cout << " Ovo je moj posluzitelj: "<< sa.toString() 
            << "\n" << "-------------------------------------------" << endl;
        //učitavanje liste relay poslužitelja
        //i za svakog od njih provjera trenutne dostupnosti(Ping - Pong)
    cout << "Faza ucitavanja liste relay posluzitelja i testiranje dostupnosti" << endl << endl;
    ProvjeraRelayPosluzitelja provjeraRelayPosluzitelja(citac.DajParametar(relayServeri));
    provjeraRelayPosluzitelja.Provjera(ds);
    cout << "\nPopis aktivnih relay posluzitelja: " << provjeraRelayPosluzitelja.DajPopisAktivnihPosluzitelja() << endl;
    //2. FAZA RADNOG REŽIMA
    timeSpanZaPrijem.assign(0, 0);
    ds.setReceiveTimeout(timeSpanZaPrijem);
    SocketAddress posiljatelj;
    cout << "Server je na prijemu . . ." << endl << endl;
    
    
    thread t1{Trosilo, 1};
    thread t2{Trosilo, 2};
   
    uint64_t brojacPunjenja = 1;
    PrijemnaPoruka prijemnaPoruka;
    PrijemnaPoruka* pokPrijemnaPoruka;

    while(1){
        ds.receiveFrom(poljeZaPrijem, sizeof(poljeZaPrijem), posiljatelj);
        cout << "Posiljatelj: " << posiljatelj.toString() << endl;
        
        cout << "Hex: " << endl;
        for(int i = 1; i<=30; i++){
            cout << i << " ";
        }
        int BB;
        cout << endl << hex;
        for(int i = 0; i<30; i++){
            BB = (int)poljeZaPrijem[i];
            cout << BB << " ";
        }
        cout << endl << dec; 
        
        pokPrijemnaPoruka = (PrijemnaPoruka*)&poljeZaPrijem[0];
        prijemnaPoruka = *pokPrijemnaPoruka;
        cout << "Punjac je napravio " << brojacPunjenja++
              << ".poruku, " << hex
              << prijemnaPoruka.identifikatorStrujanja << dec << endl;
        cirkularniBafer.Dodaj(prijemnaPoruka);
        

        
    }

    t1.join();
    t2.join();
    



    //testiranje
    SocketAddress saTest("192.168.005.104", 11000);
    char buffer[1024];

    ds.sendTo(buffer, 1024, saTest);
    try
    {
        ds.receiveFrom(buffer, sizeof(buffer), posiljatelj);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    cout << posiljatelj.toString() << ":" << buffer << endl;
    
    cout << "\nUnesi znak za kraj programa: ";
    char ooo;
    cin >> ooo;
    
    return 0;
}