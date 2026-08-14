#include "etherport.h"

#include <type_traits>

static_assert(std::has_virtual_destructor<EthernetClient>::value,
              "EthernetClient must support polymorphic deletion");

int main() {
  EthernetClient *client = new EthernetClientSsl();
  delete client;
  return 0;
}
