#pragma once


#include "Observer.h"
#include "concurrency/Lock.h"
#include "concurrency/LockGuard.h"
#include "GeoCoord.h"
#include "GPSStatus.h"


namespace meshtastic {

class Heading :
    public Observable<float>,
    public Observer<const meshtastic::GPSStatus *>
{
    struct Position
    {
        int32_t latitude;
        int32_t longitude;
    };

public:

    float offerPosition(Position pos)
    {
        concurrency::LockGuard guard(&lock);
        float heading;

        if (!hasRealHeading)
        {
            if (!hasLastPosition) {
                hasLastPosition = true;
            } else {
                notifyObservers(
                    heading = GeoCoord::bearing(
                        lastPosition.latitude,
                        lastPosition.longitude,
                        pos.latitude,
                        pos.longitude
                    )
                );
            }

            lastPosition = pos;
        }

        return heading;
    }

    float offerHeading(float heading)
    {
        concurrency::LockGuard guard(&lock);

        hasRealHeading = true;
        notifyObservers(heading);
        return heading;
    }


    int onNotify(const meshtastic::GPSStatus * status)
    {
        return offerPosition(
            {
                .latitude = status->getLatitude(),
                .longitude = status->getLongitude(),
            }
        );
    }

private:

    concurrency::Lock lock;

    bool hasLastPosition = false;
    bool hasRealHeading = false;

    Position lastPosition = {
        .latitude = INT32_MAX,
        .longitude = INT32_MAX,
    };

};

}

