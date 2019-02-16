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
#include<map>
#include<chrono>
#include "Poco/Timestamp.h"
#include "Poco/DateTimeFormatter.h"
#include "Poco/DateTimeFormat.h"
using namespace std;
using namespace Poco::Net;
using Poco::Timestamp;
using Poco::DateTimeFormatter;
using Poco::DateTimeFormat;

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

class VrijemeUpisa{
    public:
    Timestamp vrijemeUpisa;
    uint64_t identifikatorStrujanja;
    void Ispis(){
        cout << "Vrijeme upisa: " << DateTimeFormatter::format(vrijemeUpisa, DateTimeFormat::SORTABLE_FORMAT)
             << ", Ident.strujanja: " << identifikatorStrujanja << endl;
    }
};

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

    uint64_t ObrisiNajstarijiAkoJeProsloViseOd(int sekunde){
        std::unique_lock<decltype(brava)> zasun(brava);
        daLiJePrazan.wait(zasun, [this](){ return broj != 0; });

        //vadi element iz niza:
        VrijemeUpisa rez = niz[pocetak];
        if(rez.vrijemeUpisa.isElapsed(sekunde)){
            pocetak = ++pocetak % kapacitet;
            --broj;
            cout << "Obrisao sam:"
                 << DateTimeFormatter::format(rez.vrijemeUpisa, DateTimeFormat::SORTABLE_FORMAT)
                 << ", " << rez.identifikatorStrujanja << endl;
        } 
        else
        {
            rez.identifikatorStrujanja = 0;
        }
        return rez.identifikatorStrujanja;
    }

    bool NadjiIIdentifikatorStrujanjaStaviNaNulu(uint64_t identStrujanja){
        bool stavioNaNulu = false;
        std::unique_lock<decltype(brava)> zasun(brava);
        if(broj == 0) return stavioNaNulu;
        else{
            size_t pozicija = pocetak;
            size_t brojac = 0;
            VrijemeUpisa rez;
            while( broj >= brojac ){
                rez = niz[pozicija];
                if(rez.identifikatorStrujanja == identStrujanja){
                    niz[pozicija].identifikatorStrujanja = 22;
                    stavioNaNulu = true;
                    return stavioNaNulu;
                }
                pozicija++;
                brojac++;
            }
        }
        return stavioNaNulu;
    }
    void IspisCirkularnogSpremnika(){
        std::unique_lock<decltype(brava)> zasun(brava);
        if(broj == 0) {
            cout << "\tNema elemenata za ispis" << endl << endl;    
        }
        else{
            VrijemeUpisa rez;
            size_t pozicija = pocetak;
            size_t brojac = 0;
            
            while( broj > brojac ){
                rez = niz[pozicija];
                rez.Ispis();
                
                pozicija++;
                brojac++;
            }    
        }
    }
private:
    //pomoćna funkcija za izračunavanje indeksa kraja niza
    size_t DajKraj(){
        return (pocetak + broj) % kapacitet;
    }
};

struct IdentifikacijaSocketa{
    uint8_t tipArdese;
    uint32_t IPAdresa;
    uint16_t port;
};

struct PrijemnaPoruka{
    uint8_t tipPoruke;
    uint64_t identifikatorStrujanja;
    IdentifikacijaSocketa javnaAdresa;
    IdentifikacijaSocketa lokalnaAdresa;
};

KruzniSpremnik<PrijemnaPoruka, 100> cirkularniBafer; //gobalni spremnik ulaznih poruka
KruzniSpremnik<VrijemeUpisa, 10000> cirkularniSpremnikVremenaUpisa; //spremnik vremena upisa sa id.brojem
Poco::ByteOrder byteOrderMoj;

enum konfParametri {
        IPadresa, port, relayServeri, nekiNoviParametar
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

map<u_int64_t, string> registracija;
UcitavanjeKonfiguracije citac;

class PorukaMajstor
{
private:
    string pingPoruka;
    PrijemnaPoruka porukaZaObradu;
    struct StreamRegistredStruktura {
        u_int8_t tipPoruke;
        u_int64_t identifikatorStrujanja;
        u_int16_t TTL_u_sekundama;
        uint8_t tipJavneAdrese;
        uint32_t javnaIPAdresa;
        uint16_t javniBrojPorta;
    } streamRegistred;
    struct IdentifierNotUsableStruktura {
        u_int8_t tipPoruke;
        u_int64_t identifikatorStrujanja;
        uint8_t tipJavneAdrese;
        uint32_t javnaIPAdresa;
        uint16_t javniBrojPorta;
    } identifierNotUsable;

public:
    PorukaMajstor() { }
    ~PorukaMajstor() { }
    string Ping(){
        uint8_t tipPoruke = 1;
        pingPoruka.append(to_string(tipPoruke));
        pingPoruka.append("Ovo je ping poruka");
        return pingPoruka;
    }
    
    StreamRegistredStruktura* PorukaStreamRegistred(){
        streamRegistred.tipPoruke = MSG_STREAM_REGISTERED;
        streamRegistred.identifikatorStrujanja = 
            byteOrderMoj.toNetwork(porukaZaObradu.identifikatorStrujanja);
        streamRegistred.TTL_u_sekundama = byteOrderMoj.toNetwork(120);
        streamRegistred.tipJavneAdrese = 1;
        streamRegistred.javnaIPAdresa = porukaZaObradu.javnaAdresa.IPAdresa;
        streamRegistred.javniBrojPorta = porukaZaObradu.javnaAdresa.port;
        
        return &streamRegistred;
    }

    IdentifierNotUsableStruktura* PorukaIdentifierNotUsable(){
        identifierNotUsable.tipPoruke = MSG_IDENTIFIER_NOT_USABLE;
        identifierNotUsable.identifikatorStrujanja = 
            byteOrderMoj.toNetwork(porukaZaObradu.identifikatorStrujanja);
        identifierNotUsable.tipJavneAdrese = 1;
        identifierNotUsable.javnaIPAdresa = porukaZaObradu.javnaAdresa.IPAdresa;
        identifierNotUsable.javniBrojPorta = porukaZaObradu.javnaAdresa.port;

        return &identifierNotUsable;
    }

    void obradaPoruke(PrijemnaPoruka poruka){
        porukaZaObradu = poruka;
        char polje[1024];
        string string3;
        std::pair<std::_Rb_tree_iterator<std::pair<const long unsigned int, 
             std::__cxx11::basic_string<char> > >, bool> rez;
        SocketAddress saMojaAdresa(citac.DajParametar(IPadresa), citac.DajParametar(port));
        DatagramSocket dsPorukaMaster(saMojaAdresa);
        string3 = inet_ntop(AF_INET, &porukaZaObradu.javnaAdresa.IPAdresa , polje, INET_ADDRSTRLEN);
        uint16_t brojPorta = byteOrderMoj.fromNetwork(porukaZaObradu.javnaAdresa.port);
        SocketAddress saZaOdgovor(string3, brojPorta);
        int n;
        u_char* A;
        
        switch (porukaZaObradu.tipPoruke)
        {
            case imePoruke::MSG_STREAM_ADVERTISEMENT:
                cout << "Obrada pristigle porkue MSG_STREAM_ADVERTISEMENT" << endl;
                porukaZaObradu.identifikatorStrujanja = 
                    byteOrderMoj.fromNetwork(porukaZaObradu.identifikatorStrujanja);
                
                
                rez = registracija.insert({porukaZaObradu.identifikatorStrujanja,
                     string3});

                
                //vrijemeRegistracije.insert({})

                if (rez.second) {       //identifikacijski broj uspješno dodan u bazu, zapisano vrijeme upisa, poslan odgovor, testirano
                    cout << "Identifikacijski broj: " << porukaZaObradu.identifikatorStrujanja
                         << " uspješno dodan u bazu." << endl;
                    
                    VrijemeUpisa vrijemeUpisa;
                    vrijemeUpisa.vrijemeUpisa.update();
                    vrijemeUpisa.identifikatorStrujanja = porukaZaObradu.identifikatorStrujanja;
                    cirkularniSpremnikVremenaUpisa.Dodaj(vrijemeUpisa);
                    cout << "Zapisano vrijeme upisa: " << DateTimeFormatter::format(vrijemeUpisa.vrijemeUpisa, DateTimeFormat::SORTABLE_FORMAT) << ", "
                         << vrijemeUpisa.identifikatorStrujanja << endl;
                    
                    A = (u_char*)PorukaStreamRegistred();
                    n = dsPorukaMaster.sendTo(A, 1024, saZaOdgovor);
                    cout << "Posiljatelju poslana poruka MSG_STREAM_REGISTERED" << endl
                        << "____________________________________________________" << endl;
                }
                else                    
                {   //identifikacijski broj već postoji u bazi, ažurirano vrijeme upisa, poslan odgovor
                    if( registracija[porukaZaObradu.identifikatorStrujanja] == string3){
                        cout << "Identifikacijski broj: " << porukaZaObradu.identifikatorStrujanja
                             << " vec postoji u bazi." << endl;
                        
                        if(cirkularniSpremnikVremenaUpisa.NadjiIIdentifikatorStrujanjaStaviNaNulu   //brisanje zapisa
                            (porukaZaObradu.identifikatorStrujanja) == false){                      //id.zap. = 22
                                cout << "Za zadani identifikator nema vremena zapisa!!!" << endl;
                            };

                        VrijemeUpisa vrijemeUpisa;
                        vrijemeUpisa.vrijemeUpisa.update();
                        vrijemeUpisa.identifikatorStrujanja = porukaZaObradu.identifikatorStrujanja;
                        cirkularniSpremnikVremenaUpisa.Dodaj(vrijemeUpisa);
                        cout << "Zapisano vrijeme upisa: " 
                             << DateTimeFormatter::format(vrijemeUpisa.vrijemeUpisa, DateTimeFormat::SORTABLE_FORMAT)
                             << ", " << vrijemeUpisa.identifikatorStrujanja << endl;

                        

                        A = (u_char*)PorukaStreamRegistred();
                        n = dsPorukaMaster.sendTo(A, 1024, saZaOdgovor);
                        cout << "Posiljatelju poslana poruka MSG_STREAM_REGISTERED za vec "
                             << "prije registrirani zapis i azurirano je vrijeme upisa u bazu" << endl
                             << "____________________________________________________" << endl; 
                    } else {    //identifikacijski broj je već u bazi sa drugom IP adresom
                            A = (u_char*)PorukaIdentifierNotUsable();
                            n = dsPorukaMaster.sendTo(A, 1024, saZaOdgovor);
                            cout << "Posiljatelju poslana poruka MSG_IDENTIFIER_NOT_USABLE" << endl
                             << "____________________________________________________" << endl;
                      }   
                }
                cout << "\n\tRegistrirani streamovi" << endl;   //ispis registriranih streamova
                for(const auto& element : registracija){
                    cout << element.first << "\t" << element.second << endl;
                }
                cout << endl;
                cout << "\tVremena upisa" << endl;    //ispis vremena upisa za posjedini stream
                cirkularniSpremnikVremenaUpisa.IspisCirkularnogSpremnika();
                cout << "-----------------------------------------------------------" << endl << endl;
                break;
            case imePoruke::MSG_STREAM_REMOVE:
                cout << "MSG_STREAM_REMOVE" << endl;
                
                break;
            case imePoruke::MSG_REQ_RELAY_LIST:
                cout << "MSG_REQ_RELAY_LIST" << endl;
                
                break;
            case imePoruke::MSG_FIND_STREAM_SOURCE:
                cout << "MSG_FIND_STREAM_SOURCE" << endl;
                
                break;
            case imePoruke::MSG_FORWARD_PLAYER_READY:
                cout << "MSG_FORWARD_PLAYER_READY" << endl;
                
                break;
            default:
                cout << "Primljena poruka nepoznatog koda" << endl;
                break;
        }
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

void Trosilo(int id){               //dretva koja prazni spremnik
    PorukaMajstor porukaMajstor;
    static int i = 1;
    while(true){
    porukaMajstor.obradaPoruke(cirkularniBafer.Sljedeci());
    cout << "Trosilo " << id << " je obradilo " << i << ". poruku" << endl;
    i++;  
    }
}

void Punjac(int n){                 //dretva koja puni spremnik
    static uint64_t brojacPunjenja = 1;
    //cirkularniBafer.Dodaj(n);
    cout << "Punjac je napravio " << brojacPunjenja++ << ".poruku" << endl;
}

int main()
{
    const int vrijemeCekanjaUSecReceiveFrom = 1;
    const int vrijemeCekanjaUMiliSecReceiveFrom = 0;
    Poco::Timespan timeSpanZaPrijem;
    timeSpanZaPrijem.assign(vrijemeCekanjaUSecReceiveFrom, vrijemeCekanjaUMiliSecReceiveFrom);
    u_char poljeZaPrijem[1032];

    //1. FAZA INICIJALIZACIJE
        //učitavanje parametara iz konfiguracijske datoteke u objekt citac
    
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
    
    int i = 0;
    while(true){
        
        try
        {
            ds.receiveFrom(poljeZaPrijem, sizeof(poljeZaPrijem), posiljatelj);    
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            t1.join();
            t2.join();
            cout << "\nUnesi znak za kraj programa: ";
            char ooo;
            cin >> ooo;
        }
        
        cout << "\n\tPristigla je poruka od posiljatelja " << posiljatelj.toString() << endl;
        
        cout << "Ispis poruke po bajtovima u hexu: " << endl;
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
        
        /*const sockaddr* pokTest;
        pokTest = posiljatelj.addr();
        Poco::Net::IPAddress ddd = posiljatelj.host();
        string ss = ddd.toString();
        string sss = posiljatelj.host().toString(); */

        //u strukturu prijemnaPoruka ubacujem javnu ip adresu
        if (prijemnaPoruka.tipPoruke == MSG_STREAM_ADVERTISEMENT) {
            cout << "\n\tPrimljena poruka u mreznom obliku: " << endl;
            cout << "Tip poruke:\t\t" << (int)prijemnaPoruka.tipPoruke << endl;
            
            cout << "Identif.strujanja:\t" << prijemnaPoruka.identifikatorStrujanja 
                << ", hex: " << hex << prijemnaPoruka.identifikatorStrujanja << dec << endl;
            cout << "Tip lokalne sdrese:\t" << (int)prijemnaPoruka.javnaAdresa.tipArdese << endl;
            cout << "Lokalna IP adresa:\t" << prijemnaPoruka.javnaAdresa.IPAdresa 
                << ", hex: " << hex << prijemnaPoruka.javnaAdresa.IPAdresa << dec << endl;
            cout << "Lokalni broj porta:\t" << prijemnaPoruka.javnaAdresa.port
                 << ", hex: " << hex << prijemnaPoruka.javnaAdresa.port << dec << endl << endl;
                 
            prijemnaPoruka.lokalnaAdresa.tipArdese = prijemnaPoruka.javnaAdresa.tipArdese;
            prijemnaPoruka.lokalnaAdresa.IPAdresa = prijemnaPoruka.javnaAdresa.IPAdresa;
            prijemnaPoruka.lokalnaAdresa.port = prijemnaPoruka.javnaAdresa.port;
            
            prijemnaPoruka.javnaAdresa.tipArdese = 1;
            int n = inet_pton(AF_INET, posiljatelj.host().toString().data(), &prijemnaPoruka.javnaAdresa.IPAdresa);
            prijemnaPoruka.javnaAdresa.port = byteOrderMoj.toNetwork(posiljatelj.port());
        }
        
        cout << "Punjac je poslao " << brojacPunjenja++ << ".poruku na obradu, " << hex
             << prijemnaPoruka.identifikatorStrujanja << dec << endl << endl;

        cirkularniBafer.Dodaj(prijemnaPoruka);
        

        i++;
    }

    t1.join();
    t2.join();
    


    /*
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
    */

    cout << "\nUnesi znak za kraj programa: ";
    char ooo;
    cin >> ooo;
    
    return 0;
}