set(COMPONENT_REQUIRES )
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_ADD_INCLUDEDIRS "../components/TinyGPS")
set(COMPONENT_SRCS "Blink.cpp")
set(COMPONENT_EMBED_TXTFILES "../res/trip.kml")

register_component()