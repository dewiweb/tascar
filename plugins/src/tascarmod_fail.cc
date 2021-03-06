#include "session.h"

class fail_t : public TASCAR::module_base_t {
public:
  fail_t( const TASCAR::module_cfg_t& cfg );
  ~fail_t();
  void update(uint32_t frame, bool running);
  void prepare( chunk_cfg_t& );
  void release();
private:
  bool failinit;
  bool failprepare;
  bool failrelease;
  bool failupdate;
};

fail_t::fail_t( const TASCAR::module_cfg_t& cfg )
  : module_base_t( cfg ),
    failinit(false),
    failprepare(false),
    failrelease(false),
    failupdate(false)
{
  GET_ATTRIBUTE_BOOL(failinit);
  GET_ATTRIBUTE_BOOL(failprepare);
  GET_ATTRIBUTE_BOOL(failrelease);
  GET_ATTRIBUTE_BOOL(failupdate);
  if( failinit )
    throw TASCAR::ErrMsg("init.");
}

void fail_t::release()
{
  if( failrelease )
    throw TASCAR::ErrMsg("release.");
  module_base_t::release();
}

void fail_t::prepare( chunk_cfg_t& cf_ )
{
  DEBUG(this);
  if( failprepare )
    throw TASCAR::ErrMsg("prepare.");
  module_base_t::prepare( cf_ );
}

fail_t::~fail_t()
{
}

void fail_t::update(uint32_t frame,bool running)
{
  if( failupdate )
    throw TASCAR::ErrMsg("update.");
}

REGISTER_MODULE(fail_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
