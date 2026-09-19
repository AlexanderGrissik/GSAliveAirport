namespace NS_GSLiveAirportMSFS
{

class GSSimObj;

struct GSSimObjUpdate {
    virtual void OnSpawned(bool ok, GSSimObj& obj) = 0;
    virtual void OnDespawned() = 0;
};

}