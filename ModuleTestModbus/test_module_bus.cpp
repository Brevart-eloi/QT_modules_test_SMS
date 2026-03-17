#include "test_module_bus.h"

test_module_bus::test_module_bus(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

	client = new QModbusTcpClient("172.29.240.1", 502, 1, this);
    client->connectToHost();

	// ajout des connexions pour les signaux du client Modbus et des boutons de l'interface utilisateur
    QObject::connect(client, &QModbusTcpClient::connected, this, &test_module_bus::onConnected);
    QObject::connect(ui.FlashCiel2On, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
	QObject::connect(ui.FlashCiel2Off, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
	QObject::connect(ui.FlashCiel1On, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
	QObject::connect(ui.FlashCiel1Off, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
	QObject::connect(ui.SirenePHOn, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
    QObject::connect(ui.SirenePHOff, SIGNAL(clicked()), this, SLOT(onButtonClicked()));
    
}

test_module_bus::~test_module_bus()
{}


// ça permet de récupérer l'adresse et la valeur à écrire à partir des propriétés de l'objet qui a déclenché le signal (le bouton cliqué) et d'appeler la fonction forceSingleCoilFC5 du client Modbus pour écrire la valeur sur le serveur Modbus à l'adresse spécifiée.
// regarder le prompt GPT ici ► https://chatgpt.com/share/69aff5db-1204-800e-88ed-af577822b7b8
void test_module_bus::onButtonClicked()
{
    QObject * obj = sender();
    quint16 address = obj->property("CoilAdress").toUInt();
    bool value = obj->property("ValueToWrite").toBool();
    client->forceSingleCoilFC5(address, value);
}

void test_module_bus::onConnected()
{
	ui.listWidget->addItem("Connected to Modbus server.");
}

