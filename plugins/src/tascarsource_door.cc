#include "sourcemod.h"
#include "acousticmodel.h"

class door_t : public TASCAR::sourcemod_base_t, private TASCAR::Acousticmodel::diffractor_t {
public:
  class data_t : public TASCAR::sourcemod_base_t::data_t {
  public:
    data_t();
    double w;
  };
  door_t(xmlpp::Element* xmlsrc);
  bool read_source(TASCAR::pos_t& prel, const std::vector<TASCAR::wave_t>& input, TASCAR::wave_t& output, sourcemod_base_t::data_t*);
  uint32_t get_num_channels();
  TASCAR::sourcemod_base_t::data_t* create_data(double srate,uint32_t fragsize);
private:
  double width;
  double height;
  double falloff;
  double distance;
  bool wndsqrt;
};

door_t::door_t(xmlpp::Element* xmlsrc)
  : TASCAR::sourcemod_base_t(xmlsrc),
    width(1),
    height(2),
    falloff(1),
    distance(1),
    wndsqrt(false)
{
  GET_ATTRIBUTE(width);
  GET_ATTRIBUTE(height);
  GET_ATTRIBUTE(falloff);
  GET_ATTRIBUTE(distance);
  GET_ATTRIBUTE_BOOL(wndsqrt);
  nonrt_set_rect( width, height );
  diffractor_t::operator+=( TASCAR::pos_t(0,-0.5*width,-0.5*height));
}

bool door_t::read_source(TASCAR::pos_t& prel, const std::vector<TASCAR::wave_t>& input, TASCAR::wave_t& output, sourcemod_base_t::data_t* sd )
{
  data_t* d((data_t*)sd);
  double dist(prel.x);
  prel -= nearest(prel);
  TASCAR::pos_t preln(prel.normal());
  double gain(std::max(0.0,preln.x));
  // calculate gain:
  // gain rule: normal hanning window or sqrt
  double gainfalloff(0.5-0.5*cos(M_PI*std::min(1.0,std::max(0.0,dist)/falloff)));
  if( wndsqrt )
    gainfalloff = sqrt( gainfalloff );
  gain *= gainfalloff;
  double dw((gain-d->w)*t_inc);
  // end gain calc
  preln *= distance;
  prel += preln;
  uint32_t N(output.size());
  float* pi(input[0].d);
  for(uint32_t k=0;k<N;++k){
    output[k] = (*pi)*(d->w+=dw);
    ++pi;
  }
  return true;
}

door_t::data_t::data_t()
  : w(0)
{
}

uint32_t door_t::get_num_channels()
{
  return 1;
}

TASCAR::sourcemod_base_t::data_t* door_t::create_data(double srate,uint32_t fragsize)
{
  return new data_t();
}

REGISTER_SOURCEMOD(door_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
