#include<iostream>
#include<string>
#include<fstream>
#include<vector>
#include<Poco/Net/DatagramSocket.h>
#include<Poco/Net/SocketAddress.h>

using namespace std;
using namespace Poco::Net;

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
        //tokPremaKonfiguraciji.getline(cijeliRed, 10000);
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


class provjeraRelayPosluzitelja
{
private:
    /* data */
public:
    provjeraRelayPosluzitelja() { }
    ~provjeraRelayPosluzitelja() { }


};

int main()
{
    //1. FAZA INICIJALIZACIJE
    //učitavanje parametara iz konfiguracijske datoteke
    UcitavanjeKonfiguracije citac;
    citac.IspisiSveParametre();
    cout << citac.DajParametar(IPadresa) << endl;
    //priprema posluzitelja za komunikaciju
    SocketAddress sa(citac.DajParametar(IPadresa), citac.DajParametar(port));
    DatagramSocket ds(sa);
    char buffer[1024];
    cout << " Ovo je moj posluzitelj: "<< sa.toString()  << endl;
    //učitavanje liste relay poslužitelja
    cout << "Faza ucitavanja liste relay posluzitelja" << endl;
    cout << citac.DajParametar(relayServeri) << endl;
    //za svakog od njih provjera trenutne dostupnosti

    //2. FAZA RADNOG REŽIMA

    //test
    

    SocketAddress posiljatelj;
    ds.receiveFrom(buffer, sizeof(buffer), posiljatelj);
    cout << posiljatelj.toString() << ":" << buffer << endl;


    cout << "\nUnesi znak: ";
    char ooo;
    cin >> ooo;
    
    return 0;
}