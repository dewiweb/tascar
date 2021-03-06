#include "session.h"
#include <complex>
#include <mutex>
#include <gtkmm.h>
#include <gtkmm/window.h>

class geopresets_t : public TASCAR::actor_module_t {
public:
  geopresets_t( const TASCAR::module_cfg_t& cfg );
  ~geopresets_t();
  void update(uint32_t frame, bool );
  static int osc_setpreset(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void setpreset( const std::string& );
  static int osc_addposition(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void addposition( const std::string&, const TASCAR::pos_t& pos );
  static int osc_addorientation(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void addorientation( const std::string&, const TASCAR::zyx_euler_t& ori );
  void add_to_list(const std::string& preset);
private:
  double time;
  double duration;
  double current_duration;
  std::string startpreset;
  int32_t width;
  int32_t buttonheight;
  bool running;
  std::string id;
  bool enable;
  std::string preset;
  std::map<std::string,TASCAR::pos_t> positions;
  std::map<std::string,TASCAR::zyx_euler_t> orientations;
  std::vector<std::string> allpresets;
  TASCAR::zyx_euler_t current_r;
  TASCAR::pos_t current_p;
  TASCAR::zyx_euler_t new_r;
  TASCAR::pos_t new_p;
  TASCAR::zyx_euler_t old_r;
  TASCAR::pos_t old_p;
  std::complex<float> phase;
  std::complex<float> d_phase;
  std::mutex mtx;
  bool b_newpos;
  bool showgui;
  bool unlock;
  Gtk::Window* win;
  Gtk::Box* box;
  std::vector<Gtk::Button*> buttons;
  bool apply_trans;
  bool apply_rot;
};

int geopresets_t::osc_setpreset(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc==1) && (types[0]=='s') )
    ((geopresets_t*)user_data)->setpreset(&(argv[0]->s));
  return 0;
}

int geopresets_t::osc_addposition(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc==4) && (types[0]=='s') && (types[1]=='f') && (types[2]=='f') && (types[3]=='f') )
    ((geopresets_t*)user_data)->addposition(&(argv[0]->s),TASCAR::pos_t(argv[1]->f,argv[2]->f,argv[3]->f));
  return 0;
}

void geopresets_t::add_to_list(const std::string& s)
{
  bool is_in_list(false);
  for( auto it=allpresets.begin();it!=allpresets.end();++it)
    if( *it == s )
      is_in_list = true;
  if( !is_in_list ){
    allpresets.push_back(s);
    if( showgui ){
      buttons.push_back(new Gtk::Button());
      buttons.back()->set_label( s );
      buttons.back()->signal_pressed().connect(sigc::bind(sigc::mem_fun(*this,&geopresets_t::setpreset),s));
      buttons.back()->set_size_request(-1,buttonheight);
      box->add(*(buttons.back()));
      box->show_all();
    }
  }
}

void geopresets_t::addposition( const std::string& s, const TASCAR::pos_t& pos )
{
  mtx.lock();
  positions[s] = pos;
  b_newpos = true;
  add_to_list(s);
  mtx.unlock();
}

int geopresets_t::osc_addorientation(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  if( user_data && (argc==4) && (types[0]=='s') && (types[1]=='f') && (types[2]=='f') && (types[3]=='f') )
    ((geopresets_t*)user_data)->addorientation(&(argv[0]->s),
                                               TASCAR::zyx_euler_t(DEG2RAD * argv[1]->f,DEG2RAD * argv[2]->f,DEG2RAD * argv[3]->f));
  return 0;
}

void geopresets_t::addorientation( const std::string& s, const TASCAR::zyx_euler_t& pos )
{
  mtx.lock();
  orientations[s] = pos;
  b_newpos = true;
  add_to_list(s);
  mtx.unlock();
}

void geopresets_t::setpreset( const std::string& s )
{
  mtx.lock();
  preset = s;
  b_newpos = true;
  mtx.unlock();
}


geopresets_t::geopresets_t( const TASCAR::module_cfg_t& cfg )
  : actor_module_t( cfg ),
    time(0),
    duration(2.0),
    current_duration(duration),
    width(200),
    buttonheight(-1),
    running(false),
    enable(true),
    phase(1.0f),
    d_phase(1.0f),
    b_newpos(false),
    showgui(false),
    unlock(false),
    win(NULL),
    apply_trans(false),
    apply_rot(false)
{
  GET_ATTRIBUTE(duration);
  GET_ATTRIBUTE_BOOL(enable);
  GET_ATTRIBUTE(id);
  GET_ATTRIBUTE(startpreset);
  GET_ATTRIBUTE_BOOL(unlock);
  if( id.empty() )
    id = "geopresets";
  GET_ATTRIBUTE_BOOL(showgui);
  GET_ATTRIBUTE(width);
  GET_ATTRIBUTE(buttonheight);
  xmlpp::Node::NodeList subnodes = e->get_children();
  for(xmlpp::Node::NodeList::iterator sn=subnodes.begin();sn!=subnodes.end();++sn){
    xmlpp::Element* sne(dynamic_cast<xmlpp::Element*>(*sn));
    if( sne && ( sne->get_name() == "preset")){
      xml_element_t pres(sne);
      std::string name;
      pres.get_attribute("name",name);
      allpresets.push_back(name);
      if( pres.has_attribute("position") ){
        TASCAR::pos_t pos;
        pres.get_attribute("position",pos);
        positions[name] = pos;
      }
      if( pres.has_attribute("orientation") ){
        TASCAR::pos_t pos;
        pres.get_attribute("orientation",pos);
	pos *= DEG2RAD;
        orientations[name] = TASCAR::zyx_euler_t(pos.x,pos.y,pos.z);
      }
    }
  }
  session->add_method("/"+id,"s",&geopresets_t::osc_setpreset,this);
  session->add_method("/"+id+"/addposition","sfff",&geopresets_t::osc_addposition,this);
  session->add_method("/"+id+"/addorientation","sfff",&geopresets_t::osc_addorientation,this);
  session->add_double("/"+id+"/duration",&duration);
  session->add_bool("/"+id+"/enable",&enable);
  session->add_bool("/"+id+"/unlock",&unlock);
  setpreset(startpreset);
  if( showgui ){
    win = new Gtk::Window();
    win->set_size_request(width,-1);
    box = new Gtk::Box(Gtk::ORIENTATION_VERTICAL);
    win->add(*box);
    for( auto p=allpresets.begin();p!=allpresets.end();++p){
      buttons.push_back(new Gtk::Button());
      buttons.back()->set_label( *p );
      buttons.back()->signal_pressed().connect(sigc::bind(sigc::mem_fun(*this,&geopresets_t::setpreset),*p));
      buttons.back()->set_size_request(-1,buttonheight);
      box->add(*(buttons.back()));
    }
    win->show_all();
  }
}

geopresets_t::~geopresets_t()
{
  if( showgui ){
    for( auto p=buttons.begin();p!=buttons.end();++p)
      delete (*p);
    delete box;
    delete win;
  }
}

void geopresets_t::update(uint32_t tp_frame,bool tp_rolling)
{
  if( !enable )
    return;
  if( b_newpos ){
    if( mtx.try_lock() ){
      apply_trans = false;
      apply_rot = false;
      current_duration = duration;
      new_p = old_p = current_p;
      new_r = old_r = current_r;
      auto pos_it(positions.find(preset));
      if( pos_it != positions.end() ){
        apply_trans = true;
        new_p = pos_it->second;
      }
      auto rot_it(orientations.find(preset));
      if( rot_it != orientations.end() ){
        apply_rot = true;
        new_r = rot_it->second;
      }
      const std::complex<double> i(0, 1);
      d_phase = std::exp( i*M_PI*t_fragment/duration );
      phase = 1.0f;
      b_newpos = false;
      running = true;
      time = 0.0f;
      mtx.unlock();
    }
  }
  if( time > current_duration ){
    time = current_duration;
    phase = -1.0f;
    running = false;
  }
  if( running ){
    time += t_fragment;
    phase *= d_phase;
  }
  // raised-cosine weights:
  float w(0.5-0.5*std::real(phase));
  float w1(1.0f-w);
  // get weighted position:
  current_p.x = w*new_p.x + w1*old_p.x;
  current_p.y = w*new_p.y + w1*old_p.y;
  current_p.z = w*new_p.z + w1*old_p.z;
  // get weighted orientation:
  current_r.x = w*new_r.x + w1*old_r.x;
  current_r.y = w*new_r.y + w1*old_r.y;
  current_r.z = w*new_r.z + w1*old_r.z;
  // apply transformation:
  if( (!unlock) || running )
    for(std::vector<TASCAR::named_object_t>::iterator iobj=obj.begin();iobj!=obj.end();++iobj){
      if( apply_rot )
        iobj->obj->dorientation = current_r;
      if( apply_trans )
        iobj->obj->dlocation = current_p;
    }
  else
    for(std::vector<TASCAR::named_object_t>::iterator iobj=obj.begin();iobj!=obj.end();++iobj){
      new_r = current_r = iobj->obj->dorientation;
      new_p = current_p = iobj->obj->dlocation;
      
    }
}

REGISTER_MODULE(geopresets_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
