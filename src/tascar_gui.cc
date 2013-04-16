/**
   \file tascar_draw.cc
   \ingroup apptascar
   \brief Draw coordinates delivered via jack
   \author Giso Grimm
   \date 2012

   \section license License (GPL)

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

*/

#include <libxml++/libxml++.h>
#include <gtkmm.h>
#include <gtkmm/main.h>
#include <gtkmm/window.h>
#include <gtkmm/drawingarea.h>
#include <cairomm/context.h>
#include "tascar.h"
#include "osc_helper.h"
#include <iostream>
#include <getopt.h>

using namespace TASCAR;

class g_scene_t : public scene_t {
public:
  g_scene_t(const std::string& n, const std::string& flags);
  ~g_scene_t();
private:
  FILE* h_pipe;
};

g_scene_t::g_scene_t(const std::string& n, const std::string& flags)
 : h_pipe(NULL)
{
  read_xml(n);
  char ctmp[1024];
  sprintf(ctmp,"tascar_renderer %s -c %s 2>&1",flags.c_str(),n.c_str());
  h_pipe = popen( ctmp, "w" );
  if( !h_pipe )
    throw ErrMsg("Unable to open renderer pipe (tascar_renderer -c <filename>).");
  linearize_sounds();
  linearize_inputs();
  for( std::vector<src_object_t>::iterator i=srcobjects.begin();i!=srcobjects.end();++i)
    i->location.fill_gaps(0.25);
  //std::vector<bg_amb_t> bg_amb;
  // std::vector<diffuse_reverb_t> reverbs;
  for( std::vector<face_object_t>::iterator i=faces.begin();i!=faces.end();++i) 
    i->location.fill_gaps(0.25);
  listener.location.fill_gaps(0.25);
}

g_scene_t::~g_scene_t()
{
  pclose( h_pipe );
}

class source_ctl_t : public Gtk::Frame {
public:
  source_ctl_t(lo_address client_addr, scene_t* s, route_t* r);
  void on_mute();
  void on_solo();
  Gtk::EventBox ebox;
  Gtk::HBox box;
  Gtk::Label tlabel;
  Gtk::Label label;
  Gtk::ToggleButton mute;
  Gtk::ToggleButton solo;
  lo_address client_addr_;
  std::string name_;
  scene_t* scene_;
  route_t* route_;
};

source_ctl_t::source_ctl_t(lo_address client_addr, scene_t* s, route_t* r)
  : mute("M"),solo("S"),client_addr_(client_addr),name_(r->get_name()),scene_(s),route_(r)
{
  ebox.add( box );
  label.set_text(r->get_name());
  if( dynamic_cast<face_object_t*>(r))
    tlabel.set_text("mir");
  if( dynamic_cast<src_object_t*>(r))
    tlabel.set_text("src");
  if( dynamic_cast<diffuse_reverb_t*>(r))
    tlabel.set_text("rvb");
  box.pack_start( tlabel, Gtk::PACK_SHRINK );
  box.pack_start( label, Gtk::PACK_EXPAND_PADDING );
  box.pack_start( mute, Gtk::PACK_SHRINK );
  box.pack_start( solo, Gtk::PACK_SHRINK );
  mute.set_active(r->get_mute());
  solo.set_active(r->get_solo());
#ifdef GTKMM30
  Gdk::RGBA col;
  col.set_rgba_u(244*256,232*256,58*256);
  mute.override_background_color(col,Gtk::STATE_FLAG_ACTIVE);
  col.set_rgba_u(219*256,18*256,18*256);
  solo.override_background_color(col,Gtk::STATE_FLAG_ACTIVE);
  if( object_t* o=dynamic_cast<object_t*>(r) ){
    rgb_color_t c(o->color);
    col.set_rgba(c.r,c.g,c.b,0.3);
    ebox.override_background_color(col);
  }
#else
  Gdk::Color col;
  col.set_rgb(244*256,232*256,58*256);
  mute.modify_bg(Gtk::STATE_ACTIVE,col);
  mute.modify_bg(Gtk::STATE_PRELIGHT,col);
  mute.modify_bg(Gtk::STATE_SELECTED,col);
  col.set_rgb(244*256,30*256,30*256);
  solo.modify_bg(Gtk::STATE_ACTIVE,col);
  solo.modify_bg(Gtk::STATE_PRELIGHT,col);
  if( object_t* o=dynamic_cast<object_t*>(r) ){
    rgb_color_t c(o->color);
    col.set_rgb_p(0.5+0.3*c.r,0.5+0.3*c.g,0.5+0.3*c.b);
    //ebox.modify_bg(Gtk::STATE_ACTIVE,col);
    ebox.modify_bg(Gtk::STATE_NORMAL,col);
  }
#endif
  add(ebox);
  mute.signal_clicked().connect(sigc::mem_fun(*this,&source_ctl_t::on_mute));
  solo.signal_clicked().connect(sigc::mem_fun(*this,&source_ctl_t::on_solo));
}

void source_ctl_t::on_mute()
{
  bool m(mute.get_active());
  lo_send(client_addr_,"/mute","si",name_.c_str(),m);
  scene_->set_mute(name_,m);
}

void source_ctl_t::on_solo()
{
  bool m(solo.get_active());
  lo_send(client_addr_,"/solo","si",name_.c_str(),m);
  scene_->set_solo(name_,m);
}

class source_panel_t : public Gtk::ScrolledWindow {
public:
  source_panel_t(lo_address client_addr);
  void set_scene(scene_t*);
  std::vector<source_ctl_t*> vbuttons;
  Gtk::VBox box;
  lo_address client_addr_;
};

source_panel_t::source_panel_t(lo_address client_addr)
 : client_addr_(client_addr)
{
  set_size_request( 300, -1 );
  add(box);
}

void source_panel_t::set_scene(scene_t* s)
{
  for( unsigned int k=0;k<vbuttons.size();k++){
    box.remove(*(vbuttons[k]));
    delete vbuttons[k];
  }
  vbuttons.clear();
  if( s ){
    //vbuttons.push_back(new source_ctl_t(client_addr_,s,&(s->listener)));
    for( unsigned int k=0;k<s->bg_amb.size();k++)
      vbuttons.push_back(new source_ctl_t(client_addr_,s,&(s->bg_amb[k])));
    for( unsigned int k=0;k<s->srcobjects.size();k++)
      vbuttons.push_back(new source_ctl_t(client_addr_,s,&(s->srcobjects[k])));
    for( unsigned int k=0;k<s->faces.size();k++)
      vbuttons.push_back(new source_ctl_t(client_addr_,s,&(s->faces[k])));
    for( unsigned int k=0;k<s->reverbs.size();k++)
      vbuttons.push_back(new source_ctl_t(client_addr_,s,&(s->reverbs[k])));
  }
  for( unsigned int k=0;k<vbuttons.size();k++){
    box.pack_start(*(vbuttons[k]), Gtk::PACK_SHRINK);
  }
  show_all();
}

class tascar_gui_t : public osc_server_t, public jackc_transport_t
{
public:
  tascar_gui_t(const std::string& name,const std::string& oscport,const std::string& renderflags_);
  ~tascar_gui_t();
  void open_scene(const std::string& name, const std::string& flags);
  void set_time( double t ){time = t;};
  void set_scale(double s){scale = s;};
  void draw_track(const object_t& obj,Cairo::RefPtr<Cairo::Context> cr, double msize);
  void draw_src(const src_object_t& obj,Cairo::RefPtr<Cairo::Context> cr, double msize);
  void draw_listener(const listener_t& obj,Cairo::RefPtr<Cairo::Context> cr, double msize);
  void draw_room(const diffuse_reverb_t& obj,Cairo::RefPtr<Cairo::Context> cr, double msize);
  void draw_face(const face_object_t& obj,Cairo::RefPtr<Cairo::Context> cr, double msize);
  static int osc_listener_orientation(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int osc_listener_position(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  static int set_head(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data);
  void set_head(double rot){headrot = rot;};
  virtual int process(jack_nframes_t n,const std::vector<float*>& input,
                      const std::vector<float*>& output, 
                      uint32_t tp_frame, bool tp_rolling);
protected:
  virtual bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr);
#ifdef GTKMM24
  virtual bool on_expose_event(GdkEventExpose* event);
#endif
  bool on_timeout();
  bool on_timeout_blink();
  void on_time_changed();
  void on_tp_start(){
    if( scene && (guitime >= scene->duration) )
      tp_locate(0.0);
    tp_start();
  };
  void on_tp_stop(){tp_stop();};
  void on_tp_rewind();
  void on_tp_time_p(){tp_locate(guitime+0.1);};
  void on_tp_time_pp(){tp_locate(guitime+1.0);};
  void on_tp_time_m(){tp_locate(guitime-0.1);};
  void on_tp_time_mm(){tp_locate(guitime-1.0);};
  void on_reload();
  void on_view_p();
  void on_view_m();
  void on_range_selected();
  double scale;
  double time;
  double guitime;
  double headrot;
  //Gtk::HBox hbox;
  Gtk::Button button_tp_stop;
  Gtk::Button button_tp_start;
  Gtk::Button button_tp_rewind;
  Gtk::Button button_tp_time_p;
  Gtk::Button button_tp_time_pp;
  Gtk::Button button_tp_time_m;
  Gtk::Button button_tp_time_mm;
  Gtk::Button button_reload;
  Gtk::Button button_view_p;
  Gtk::Button button_view_m;
  Gtk::ComboBoxText rangeselector;
private:
  lo_address client_addr;
public:
  Gtk::DrawingArea wdg_scenemap;
  source_panel_t wdg_source;
  Gtk::VBox wdg_vertmain;
  Gtk::HBox wdg_source_and_map;
  Gtk::HBox wdg_transport_box;
  Gtk::HBox wdg_file_ui_box;
#ifdef GTKMM30
  Gtk::Scale timescale;
#else
  Gtk::HScale timescale;
#endif
private:
  pthread_mutex_t mtx_scene;
  g_scene_t* scene;
  std::string filename;
  std::string renderflags;
  std::vector<TASCAR::pos_t> roomnodes;
  bool blink;
  int32_t selected_range;
};

void tascar_gui_t::on_tp_rewind()
{
  if( scene && (selected_range >= 0) && (selected_range<(int32_t)(scene->ranges.size())) ){
    tp_locate(scene->ranges[selected_range].start);
  }else{
    tp_locate(0.0);
  }
}

int tascar_gui_t::osc_listener_orientation(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  tascar_gui_t* h((tascar_gui_t*)user_data);
  if( h ){
    //lo_send_message(h->client_addr,path,msg);
    zyx_euler_t r;
    if( (argc == 3) && (types[0]=='f') && (types[1]=='f') && (types[2]=='f') ){
      r.z = DEG2RAD*argv[0]->f;
      r.y = DEG2RAD*argv[1]->f;
      r.x = DEG2RAD*argv[2]->f;
    }
    if( (argc == 1) && (types[0]=='f') ){
      r.z = DEG2RAD*argv[0]->f;
    }
    if( h->scene )
      h->scene->listener_orientation(r);
    return 0;
  }
  return 1;
}

int tascar_gui_t::osc_listener_position(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  tascar_gui_t* h((tascar_gui_t*)user_data);
  if( h ){
    //lo_send_message(h->client_addr,path,msg);
    pos_t r;
    if( (argc == 3) && (types[0]=='f') && (types[1]=='f') && (types[2]=='f') ){
      r.x = argv[0]->f;
      r.y = argv[1]->f;
      r.z = argv[2]->f;
    }
    if( (argc == 2) && (types[0]=='f') && (types[1]=='f') ){
      r.x = argv[0]->f;
      r.y = argv[1]->f;
    }
    if( h->scene )
      h->scene->listener_position(r);
    return 0;
  }
  return 1;
}

void tascar_gui_t::on_time_changed()
{
  double ltime(timescale.get_value());
  if( ltime != guitime ){
    tp_locate(ltime);
  }
}

void tascar_gui_t::on_range_selected()
{
  int32_t nr(-1);
  double start_time(0);
  if( scene ){
    std::string rg(rangeselector.get_active_text());
    
    for(unsigned int k=0;k<scene->ranges.size();k++)
      if( scene->ranges[k].name == rg ){
        nr = k;
        start_time = scene->ranges[k].start;
      }
  }
  selected_range = nr;
  tp_locate(start_time);
}

void tascar_gui_t::draw_track(const object_t& obj,Cairo::RefPtr<Cairo::Context> cr, double msize)
{
  bool solo(obj.get_solo());
  cr->save();
  if( solo && blink ){
    cr->set_source_rgba(1,0,0,0.5);
    cr->set_line_width( 1.2*msize );
    for( TASCAR::track_t::const_iterator it=obj.location.begin();it!=obj.location.end();++it){
      if( it==obj.location.begin() )
        cr->move_to( it->second.x, -it->second.y );
      else
        cr->line_to( it->second.x, -it->second.y );
    }
    cr->stroke();
  }
  cr->set_source_rgb(obj.color.r, obj.color.g, obj.color.b );
  if( solo && blink )
    cr->set_line_width( 0.3*msize );
  else
    cr->set_line_width( 0.1*msize );
  for( TASCAR::track_t::const_iterator it=obj.location.begin();it!=obj.location.end();++it){
    if( it==obj.location.begin() )
      cr->move_to( it->second.x, -it->second.y );
    else
      cr->line_to( it->second.x, -it->second.y );
  }
  cr->stroke();
  cr->restore();
}

void tascar_gui_t::draw_src(const src_object_t& obj,Cairo::RefPtr<Cairo::Context> cr, double msize)
{
  bool active(obj.isactive(time));
  bool solo(obj.get_solo());
  double plot_time(time);
  if( !active ){
    msize *= 0.4;
    plot_time = std::min(std::max(plot_time,obj.starttime),obj.endtime);
  }
  pos_t p(obj.location.interp(plot_time-obj.starttime));
  cr->save();
  if( solo && blink ){
    cr->set_source_rgba(1, 0, 0, 0.5);
    cr->arc(p.x, -p.y, 3*msize, 0, PI2 );
    cr->fill();
  }
  cr->set_source_rgba(obj.color.r, obj.color.g, obj.color.b, 0.6);
  cr->arc(p.x, -p.y, msize, 0, PI2 );
  cr->fill();
  for(unsigned int k=0;k<obj.sound.size();k++){
    pos_t ps(obj.sound[k].get_pos_global(plot_time));
    //pos_t ps(obj.sound[k].get_pos(time));
    cr->arc(ps.x, -ps.y, 0.5*msize, 0, PI2 );
    cr->fill();
  }
  if( !active )
    cr->set_source_rgb(0.5, 0.5, 0.5 );
  else
    cr->set_source_rgb(0, 0, 0 );
  cr->move_to( p.x + 1.1*msize, -p.y );
  cr->show_text( obj.get_name().c_str() );
  if( active ){
    cr->set_line_width( 0.1*msize );
    cr->set_source_rgba(obj.color.r, obj.color.g, obj.color.b, 0.6);
    cr->move_to( p.x, -p.y );
    for(unsigned int k=0;k<obj.sound.size();k++){
      pos_t ps(obj.sound[k].get_pos_global(plot_time));
      //pos_t ps(obj.sound[k].get_pos(time));
      cr->line_to( ps.x, -ps.y );
    }
    cr->stroke();
  }
  cr->restore();
}

void tascar_gui_t::open_scene(const std::string& name, const std::string& flags)
{
  pthread_mutex_lock( &mtx_scene );
  if( scene )
    delete scene;
  scene = NULL;
  timescale.clear_marks();
  rangeselector.remove_all();
  rangeselector.append("- scene -");
  rangeselector.set_active_text("- scene -");
  selected_range = -1;
  if( name.size() ){
    scene = new g_scene_t(name, flags);
    timescale.set_range(0,scene->duration);
    for(unsigned int k=0;k<scene->ranges.size();k++){
      //timescale.add_mark(scene->ranges[k].start,Gtk::POS_TOP,"<span horizontalalign=\"right\">"+scene->ranges[k].name+"</span>");
      //timescale.add_mark(scene->ranges[k].start,Gtk::POS_TOP,"&gt;");
      //timescale.add_mark(scene->ranges[k].end,Gtk::POS_TOP,"&lt;");
      timescale.add_mark(scene->ranges[k].start,Gtk::POS_BOTTOM,"");
      timescale.add_mark(scene->ranges[k].end,Gtk::POS_BOTTOM,"");
      rangeselector.append(scene->ranges[k].name);
    }
    set_scale(scene->guiscale);
  }
  wdg_source.set_scene( scene );
  pthread_mutex_unlock( &mtx_scene );
}

void tascar_gui_t::on_reload()
{
  open_scene( filename, renderflags );
}

void tascar_gui_t::on_view_p()
{
  pthread_mutex_lock( &mtx_scene );
  if( scene ){
    scene->guiscale /= 1.2;
    set_scale(scene->guiscale);
  }
  pthread_mutex_unlock( &mtx_scene );
}

void tascar_gui_t::on_view_m()
{
  pthread_mutex_lock( &mtx_scene );
  if( scene ){
    scene->guiscale *= 1.2;
    set_scale(scene->guiscale);
  }
  pthread_mutex_unlock( &mtx_scene );
}

void tascar_gui_t::draw_listener(const listener_t& obj,Cairo::RefPtr<Cairo::Context> cr, double msize)
{
  msize *= 1.5;
  pos_t p(obj.location.interp(time-obj.starttime));
  p += obj.dlocation;
  zyx_euler_t o(obj.orientation.interp(time-obj.starttime));
  o += obj.dorientation;
  //DEBUG(o.print());
  o.z += headrot;
  pos_t p1(1.8*msize,-0.6*msize,0);
  pos_t p2(2.9*msize,0,0);
  pos_t p3(1.8*msize,0.6*msize,0);
  pos_t p4(-0.5*msize,2.3*msize,0);
  pos_t p5(0,1.7*msize,0);
  pos_t p6(0.5*msize,2.3*msize,0);
  pos_t p7(-0.5*msize,-2.3*msize,0);
  pos_t p8(0,-1.7*msize,0);
  pos_t p9(0.5*msize,-2.3*msize,0);
  p1 *= o;
  p2 *= o;
  p3 *= o;
  p4 *= o;
  p5 *= o;
  p6 *= o;
  p7 *= o;
  p8 *= o;
  p9 *= o;
  p1+=p;
  p2+=p;
  p3+=p;
  p4+=p;
  p5+=p;
  p6+=p;
  p7+=p;
  p8+=p;
  p9+=p;
  cr->save();
  cr->set_line_width( 0.2*msize );
  cr->set_source_rgba(obj.color.r, obj.color.g, obj.color.b, 0.6);
  cr->move_to( p.x, -p.y );
  cr->arc(p.x, -p.y, 2*msize, 0, PI2 );
  cr->move_to( p1.x, -p1.y );
  cr->line_to( p2.x, -p2.y );
  cr->line_to( p3.x, -p3.y );
  cr->move_to( p4.x, -p4.y );
  cr->line_to( p5.x, -p5.y );
  cr->line_to( p6.x, -p6.y );
  cr->move_to( p7.x, -p7.y );
  cr->line_to( p8.x, -p8.y );
  cr->line_to( p9.x, -p9.y );
  //cr->fill();
  cr->stroke();
  cr->set_source_rgb(0, 0, 0 );
  cr->move_to( p.x + 3.1*msize, -p.y );
  cr->show_text( obj.get_name().c_str() );
  cr->restore();
}

void tascar_gui_t::draw_room(const TASCAR::diffuse_reverb_t& reverb,Cairo::RefPtr<Cairo::Context> cr, double msize)
{
  bool solo(reverb.get_solo());
  std::vector<pos_t> roomnodes(8,reverb.center);
  roomnodes[0].x -= 0.5*reverb.size.x;
  roomnodes[1].x += 0.5*reverb.size.x;
  roomnodes[2].x += 0.5*reverb.size.x;
  roomnodes[3].x -= 0.5*reverb.size.x;
  roomnodes[4].x -= 0.5*reverb.size.x;
  roomnodes[5].x += 0.5*reverb.size.x;
  roomnodes[6].x += 0.5*reverb.size.x;
  roomnodes[7].x -= 0.5*reverb.size.x;
  roomnodes[0].y -= 0.5*reverb.size.y;
  roomnodes[1].y -= 0.5*reverb.size.y;
  roomnodes[2].y += 0.5*reverb.size.y;
  roomnodes[3].y += 0.5*reverb.size.y;
  roomnodes[4].y -= 0.5*reverb.size.y;
  roomnodes[5].y -= 0.5*reverb.size.y;
  roomnodes[6].y += 0.5*reverb.size.y;
  roomnodes[7].y += 0.5*reverb.size.y;
  roomnodes[0].z -= 0.5*reverb.size.z;
  roomnodes[1].z -= 0.5*reverb.size.z;
  roomnodes[2].z -= 0.5*reverb.size.z;
  roomnodes[3].z -= 0.5*reverb.size.z;
  roomnodes[4].z += 0.5*reverb.size.z;
  roomnodes[5].z += 0.5*reverb.size.z;
  roomnodes[6].z += 0.5*reverb.size.z;
  roomnodes[7].z += 0.5*reverb.size.z;
  for(unsigned int k=0;k<8;k++)
    roomnodes[k] *= reverb.orientation;
  cr->save();
  if( solo && blink )
    cr->set_line_width( 0.6*msize );
  else
    cr->set_line_width( 0.1*msize );
  cr->set_source_rgba(0,0,0,0.6);
  cr->move_to( roomnodes[0].x, -roomnodes[0].y );
  cr->line_to( roomnodes[1].x, -roomnodes[1].y );
  cr->line_to( roomnodes[2].x, -roomnodes[2].y );
  cr->line_to( roomnodes[3].x, -roomnodes[3].y );
  cr->line_to( roomnodes[0].x, -roomnodes[0].y );
  cr->move_to( roomnodes[4].x, -roomnodes[4].y );
  cr->line_to( roomnodes[5].x, -roomnodes[5].y );
  cr->line_to( roomnodes[6].x, -roomnodes[6].y );
  cr->line_to( roomnodes[7].x, -roomnodes[7].y );
  cr->line_to( roomnodes[4].x, -roomnodes[4].y );
  for(unsigned int k=0;k<4;k++){
    cr->move_to( roomnodes[k].x, -roomnodes[k].y );
    cr->line_to( roomnodes[k+4].x, -roomnodes[k+4].y );
  }
  cr->stroke();
  cr->set_source_rgb(0, 0, 0 );
  cr->move_to( roomnodes[0].x + 0.1*msize, -roomnodes[0].y );
  cr->show_text( reverb.get_name().c_str() );
  cr->restore();
}

void tascar_gui_t::draw_face(const TASCAR::face_object_t& face,Cairo::RefPtr<Cairo::Context> cr, double msize)
{
  bool active(face.isactive(time));
  bool solo(face.get_solo());
  if( !active )
    msize*=0.5;
  std::vector<pos_t> roomnodes(6);
  roomnodes[0].y -= 0.5*face.width;
  roomnodes[1].y -= 0.5*face.width;
  roomnodes[2].y += 0.5*face.width;
  roomnodes[3].y += 0.5*face.width;
  roomnodes[0].z -= 0.5*face.height;
  roomnodes[1].z += 0.5*face.height;
  roomnodes[2].z -= 0.5*face.height;
  roomnodes[3].z += 0.5*face.height;
  roomnodes[5].x += 3*msize;
  pos_t loc(face.get_location(time));
  zyx_euler_t o(face.get_orientation(time));
  for(unsigned int k=0;k<6;k++){
    roomnodes[k] *= o;
    roomnodes[k] += loc;
  }
  cr->save();
  if( solo && blink ){
    cr->set_line_width( 1.2*msize );
    cr->set_source_rgba(1,0,0,0.5);
    cr->move_to( roomnodes[0].x, -roomnodes[0].y );
    cr->line_to( roomnodes[1].x, -roomnodes[1].y );
    cr->line_to( roomnodes[2].x, -roomnodes[2].y );
    cr->line_to( roomnodes[3].x, -roomnodes[3].y );
    cr->line_to( roomnodes[0].x, -roomnodes[0].y );
    cr->stroke();
  }
  cr->set_line_width( 0.3*msize );
  cr->set_source_rgba(face.color.r,face.color.g,face.color.b,0.6);
  cr->move_to( roomnodes[0].x, -roomnodes[0].y );
  cr->line_to( roomnodes[1].x, -roomnodes[1].y );
  cr->line_to( roomnodes[2].x, -roomnodes[2].y );
  cr->line_to( roomnodes[3].x, -roomnodes[3].y );
  cr->line_to( roomnodes[0].x, -roomnodes[0].y );
  cr->stroke();
  if( active ){
    cr->set_source_rgba(face.color.r,face.color.g,0.5+0.5*face.color.b,0.8);
    cr->move_to( roomnodes[4].x, -roomnodes[4].y );
    cr->line_to( roomnodes[5].x, -roomnodes[5].y );
    cr->stroke();
    cr->set_source_rgb(0, 0, 0 );
    cr->move_to( roomnodes[0].x + 0.1*msize, -roomnodes[0].y );
    cr->show_text( face.get_name().c_str() );
  }
  cr->restore();
}

int tascar_gui_t::set_head(const char *path, const char *types, lo_arg **argv, int argc, lo_message msg, void *user_data)
{
  tascar_gui_t* h((tascar_gui_t*)user_data);
  if( h && (argc == 1) && (types[0] == 'f') ){
    h->set_head(DEG2RAD*(argv[0]->f));
    return 0;
  }
  return 1;
}

#define CON_BUTTON(b) button_ ## b.signal_clicked().connect(sigc::mem_fun(*this,&tascar_gui_t::on_ ## b))

tascar_gui_t::tascar_gui_t(const std::string& name, const std::string& oscport, const std::string& renderflags_)
  : osc_server_t("",oscport,true),
    jackc_transport_t(name),
    scale(200),
    time(0),
    guitime(0),
    button_tp_stop("stop"),
    button_tp_start("play"),
    button_tp_rewind("rew"),
    button_tp_time_p("+"),
    button_tp_time_pp("++"),
    button_tp_time_m("-"),
    button_tp_time_mm("--"),
    button_reload("reload"),
    button_view_p("zoom +"),
    button_view_m("zoom -"),
    client_addr(lo_address_new("localhost","9877")),
    wdg_source(client_addr),
#ifdef GTKMM30
    timescale(Gtk::ORIENTATION_HORIZONTAL),
#endif
    scene(NULL),
    filename(name),
    renderflags(renderflags_),
    blink(false),
    selected_range(-1)
{
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &tascar_gui_t::on_timeout), 60 );
  Glib::signal_timeout().connect( sigc::mem_fun(*this, &tascar_gui_t::on_timeout_blink), 600 );
#ifdef GTKMM30
  wdg_scenemap.signal_draw().connect(sigc::mem_fun(*this, &tascar_gui_t::on_draw), false);
#else
  wdg_scenemap.signal_expose_event().connect(sigc::mem_fun(*this, &tascar_gui_t::on_expose_event), false);
#endif
  osc_server_t::add_method("/headrot","f",set_head,this);
  osc_server_t::add_method("/listener/pos","fff",osc_listener_position,this);
  osc_server_t::add_method("/listener/rot","fff",osc_listener_orientation,this);
  osc_server_t::add_method("/listener/pos","ff",osc_listener_position,this);
  osc_server_t::add_method("/listener/rot","f",osc_listener_orientation,this);
  wdg_file_ui_box.pack_start( button_reload, Gtk::PACK_SHRINK );
  wdg_file_ui_box.pack_start( button_view_p, Gtk::PACK_SHRINK );
  wdg_file_ui_box.pack_start( button_view_m, Gtk::PACK_SHRINK );
  wdg_file_ui_box.pack_start( rangeselector, Gtk::PACK_SHRINK );
  wdg_transport_box.pack_start( button_tp_rewind, Gtk::PACK_SHRINK );
  wdg_transport_box.pack_start( button_tp_stop, Gtk::PACK_SHRINK );
  wdg_transport_box.pack_start( button_tp_start, Gtk::PACK_SHRINK );
  wdg_transport_box.pack_start( button_tp_time_mm, Gtk::PACK_SHRINK );
  wdg_transport_box.pack_start( button_tp_time_m, Gtk::PACK_SHRINK );
  wdg_transport_box.pack_start( timescale );
  wdg_transport_box.pack_start( button_tp_time_p, Gtk::PACK_SHRINK );
  wdg_transport_box.pack_start( button_tp_time_pp, Gtk::PACK_SHRINK );
  wdg_vertmain.pack_start( wdg_file_ui_box, Gtk::PACK_SHRINK);

  wdg_source_and_map.pack_start( wdg_source, Gtk::PACK_SHRINK );
  wdg_source_and_map.pack_start( wdg_scenemap );

  wdg_vertmain.pack_start( wdg_source_and_map );
  wdg_vertmain.pack_start( wdg_transport_box, Gtk::PACK_SHRINK );
  timescale.set_range(0,100);
  timescale.set_value(0);
  timescale.signal_value_changed().connect(sigc::mem_fun(*this,
                                                         &tascar_gui_t::on_time_changed));
  rangeselector.signal_changed().connect(sigc::mem_fun(*this,
                                                        &tascar_gui_t::on_range_selected));
  CON_BUTTON(tp_rewind);
  CON_BUTTON(tp_start);
  CON_BUTTON(tp_stop);
  CON_BUTTON(tp_time_p);
  CON_BUTTON(tp_time_pp);
  CON_BUTTON(tp_time_m);
  CON_BUTTON(tp_time_mm);
  CON_BUTTON(reload);
  CON_BUTTON(view_p);
  CON_BUTTON(view_m);
  pthread_mutex_init( &mtx_scene, NULL );
  if( name.size() )
    open_scene(name,renderflags);
}

tascar_gui_t::~tascar_gui_t()
{
  pthread_mutex_trylock( &mtx_scene );
  pthread_mutex_unlock(  &mtx_scene);
  pthread_mutex_destroy( &mtx_scene );
}

#ifdef GTKMM24
bool tascar_gui_t::on_expose_event(GdkEventExpose* event)
{
  if( event ){
    // This is where we draw on the window
    Glib::RefPtr<Gdk::Window> window = wdg_scenemap.get_window();
    if(window){
      Cairo::RefPtr<Cairo::Context> cr = window->create_cairo_context();
      return on_draw(cr);
    }
  }
  return true;
}
#endif


bool tascar_gui_t::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  pthread_mutex_lock( &mtx_scene );
  try{
    Glib::RefPtr<Gdk::Window> window = wdg_scenemap.get_window();
    if(window && scene){
      Gtk::Allocation allocation = wdg_scenemap.get_allocation();
      const int width = allocation.get_width();
      const int height = allocation.get_height();
      cr->rectangle(0,0,width,height);
      cr->clip();
      cr->translate(0.5*width, 0.5*height);
      double wscale(0.5*std::min(height,width)/scale);
      double markersize(0.02*scale);
      cr->scale( wscale, wscale );
      cr->set_line_width( 0.3*markersize );
      cr->set_font_size( 2*markersize );
      double scale_len(pow(10.0,floor(log10(scale))));
      double scale_r(scale/scale_len);
      if( scale_r >= 5 )
        scale_len *=5;
      else if( scale_r >= 2 )
        scale_len *= 2;
      cr->save();
      cr->set_source_rgb( 1, 1, 1 );
      cr->paint();
      cr->restore();
      draw_track( scene->listener, cr, markersize );
      for(unsigned int k=0;k<scene->srcobjects.size();k++){
        draw_track(scene->srcobjects[k], cr, markersize );
      }
      for(unsigned int k=0;k<scene->reverbs.size();k++){
        draw_room(scene->reverbs[k], cr, markersize );
      }
      for(unsigned int k=0;k<scene->faces.size();k++){
        draw_face(scene->faces[k], cr, markersize );
      }
      draw_listener( scene->listener, cr, markersize );
      cr->set_source_rgba(0.2, 0.2, 0.2, 0.8);
      cr->move_to(-markersize, 0 );
      cr->line_to( markersize, 0 );
      cr->move_to( 0, -markersize );
      cr->line_to( 0,  markersize );
      cr->stroke();
      for(unsigned int k=0;k<scene->srcobjects.size();k++){
        draw_src(scene->srcobjects[k], cr, markersize );
      }
      cr->save();
      cr->set_source_rgb( 0, 0, 0 );
      cr->move_to( -0.9*scale,0.9*scale );
      cr->line_to( -0.9*scale,0.95*scale );
      cr->line_to( -0.9*scale+scale_len,0.95*scale );
      cr->line_to( -0.9*scale+scale_len,0.9*scale );
      cr->stroke();
      cr->move_to( -0.88*scale,0.94*scale );
      char cmp[1024];
      sprintf(cmp,"%g m",scale_len);
      cr->show_text( cmp );
      cr->restore();
    }
    guitime = time;
    timescale.set_value(guitime);
    pthread_mutex_unlock( &mtx_scene );
  }
  catch(...){
    pthread_mutex_unlock( &mtx_scene );
    throw;
  }
  return true;
}

bool tascar_gui_t::on_timeout()
{
  Glib::RefPtr<Gdk::Window> win = wdg_scenemap.get_window();
  //Glib::RefPtr<Gdk::Window> win = get_window();
  //DEBUG(win);
  if (win){
    //Gdk::Rectangle r(wdg_scenemap.get_allocation().get_x(), wdg_scenemap.get_allocation().get_y(), 
    //    	     wdg_scenemap.get_allocation().get_width(),
    //		     wdg_scenemap.get_allocation().get_height() );
    Gdk::Rectangle r(0,0, 
		     wdg_scenemap.get_allocation().get_width(),
		     wdg_scenemap.get_allocation().get_height() );
    win->invalidate_rect(r, true);
  }
  return true;
}

bool tascar_gui_t::on_timeout_blink()
{
  if( blink )
    blink = false;
  else
    blink = true;
  return true;
}

int tascar_gui_t::process(jack_nframes_t nframes,
                          const std::vector<float*>& inBuffer,
                          const std::vector<float*>& outBuffer, 
                          uint32_t tp_frame, bool tp_rolling)
{
  set_time((double)tp_frame/get_srate());
  if( scene ){
    if( (selected_range >= 0) && (selected_range < (int32_t)(scene->ranges.size())) ){
      if( (scene->ranges[selected_range].end <= time) && 
          (scene->ranges[selected_range].end + (double)nframes/(double)srate > time) )
        tp_stop();
    }else{
      if( (scene->duration <= time) && (scene->duration + (double)nframes/(double)srate > time) )
        tp_stop();
    }
  }
  return 0;
}

void usage(struct option * opt)
{
  std::cout << "Usage:\n\ntascar_gui -c configfile [options]\n\nOptions:\n\n";
  while( opt->name ){
    std::cout << "  -" << (char)(opt->val) << " " << (opt->has_arg?"#":"") <<
      "\n  --" << opt->name << (opt->has_arg?"=#":"") << "\n\n";
    opt++;
  }
}

int main(int argc, char** argv)
{
  Gtk::Main kit(argc, argv);
  Gtk::Window win;
  std::string cfgfile("");
  std::string oscport("9876");
#ifdef LINUXTRACK
  bool use_ltr(false);
  const char *options = "c:hp:l";
#else
  const char *options = "c:hp:";
#endif
  struct option long_options[] = { 
    { "config",   1, 0, 'c' },
    { "help",     0, 0, 'h' },
    { "oscport",  1, 0, 'p' },
#ifdef LINUXTRACK
    { "linuxtrack", 0, 0, 'l'},
#endif
    { 0, 0, 0, 0 }
  };
  int opt(0);
  int option_index(0);
  while( (opt = getopt_long(argc, argv, options,
                            long_options, &option_index)) != -1){
    switch(opt){
    case 'c':
      cfgfile = optarg;
      break;
    case 'h':
      usage(long_options);
      return -1;
    case 'p':
      oscport = optarg;
      break;
#ifdef LINUXTRACK
    case 'l':
      use_ltr = true;
      break;
#endif
    }
  }
  if( cfgfile.size() == 0 ){
    usage(long_options);
    return -1;
  }
  win.set_title("tascar - "+cfgfile);
#ifdef LINUXTRACK
  tascar_gui_t c(cfgfile,oscport,use_ltr?"-l":"");
#else
  tascar_gui_t c(cfgfile,oscport,"");
#endif
  win.add(c.wdg_vertmain);
  win.set_default_size(1024,768);
  win.show_all();
  c.jackc_t::activate();
  c.osc_server_t::activate();
  Gtk::Main::run(win);
  c.osc_server_t::deactivate();
  c.jackc_t::deactivate();
  return 0;
}

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */