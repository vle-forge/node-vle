#include <vle/vle.hpp>
#include <vle/manager/Manager.hpp>
#include <vle/manager/Simulation.hpp>
#include <vle/vpz/Vpz.hpp>
#include <vle/utils/Package.hpp>

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

#include <sstream>

using namespace vle;
using namespace v8;

static bool thread_init = false;

class VleWrapper : public node::ObjectWrap
{
public:
  static void Init(v8::Handle<v8::Object> exports);
  
private:
  vpz::Vpz* _vpz;

  explicit VleWrapper(const char* pkg_name, const char* file_name)
  {
    try {
      if (!thread_init) {
	Init app;
	
	thread_init = true;
      }
      utils::Package pack(pkg_name);
      std::string filepath = pack.getExpFile(file_name);
      
      _vpz = new vpz::Vpz(filepath);

      vpz::Outputs& outlst(_vpz->project().experiment().views().outputs());
      vpz::Outputs::iterator it;
      
      for (it = outlst.begin(); it != outlst.end(); ++it) {
        it->second.setLocalStream("", "storage", "vle.output");
      }
    } catch(const std::exception& e) {
      _vpz = 0;
    }
  }

  virtual ~VleWrapper()
  { if (_vpz) delete _vpz; }

  static v8::Persistent<v8::Function> constructor;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);  

  static void get_duration(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void run(const v8::FunctionCallbackInfo<v8::Value>& args);
};

Handle < Value > convert_value(const value::Value& value,
				Isolate* isolate)
{
  EscapableHandleScope scope(isolate);

  switch (value.getType()) {
  case value::Value::DOUBLE: {
    return scope.Escape(Number::New(isolate,
				    value::toDouble(value)));
  }
  case value::Value::BOOLEAN: {
    return scope.Escape(Boolean::New(isolate,
				     value::toBoolean(value)));
  }
  case value::Value::INTEGER: {
    return scope.Escape(Number::New(isolate,
				    value::toInteger(value)));
  }
  case value::Value::STRING: {
    return scope.Escape(String::NewFromUtf8(isolate,
					    value::toString(value).c_str()));
  }
  case value::Value::XMLTYPE: {
    return scope.Escape(String::NewFromUtf8(isolate,
					    value::toXml(value).c_str()));
  }
  case value::Value::MAP: {
    Handle < Object > result = Object::New(isolate);
    
    for (value::Map::const_iterator it = value.toMap().begin();
	 it != value.toMap().end(); ++it) {
      result->Set(String::NewFromUtf8(isolate, it->first.c_str()),
		  convert_value(*(it->second), isolate));
    }
    return scope.Escape(result);
  }
  case value::Value::SET: {
    Handle < Array > result = Array::New(isolate);
    unsigned int i = 0;
    
    for (value::Set::const_iterator it = value.toSet().begin();
	 it != value.toSet().end(); ++it) {
      result->Set(i, convert_value(**it, isolate));
      ++i;
    }    
    return scope.Escape(result);
  }
  case value::Value::TUPLE: {
    Handle < Array > result = Array::New(isolate);
    unsigned int i = 0;

    for(std::vector < double >::const_iterator it = value.toTuple().
	  value().begin(); it != value.toTuple().value().end(); ++it){
      result->Set(i, Number::New(isolate, *it));
      ++i;
    }
    return scope.Escape(result);
  }
  case value::Value::TABLE: {
    Handle < Array > result = Array::New(isolate);
    const value::Table& t = value.toTable();

    for (unsigned int i = 0; i < t.height(); i++){
      Handle < Array > line = Array::New(isolate);

      for (unsigned int j = 0; j < t.width(); j++){
	result->Set(j, Number::New(isolate, t.get(j,i)));
      }
      result->Set(i, line);
    }
    return scope.Escape(result);
  }
  case value::Value::MATRIX: {
    Handle < Array > result = Array::New(isolate);
    const value::Matrix& t = value.toMatrix();

    for (unsigned int i = 0; i < t.rows(); i++){
      Handle < Array > line = Array::New(isolate);

      for (unsigned int j = 0; j < t.columns(); j++){
	result->Set(j, convert_value(*t.get(j,i), isolate));
      }
      result->Set(i, line);
    }
    return scope.Escape(result);
  }
  default: {
    return scope.Escape(Null(isolate));
  }
  }
}

void split(const std::string& str, char delim, std::vector < std::string >& vec)
{
  std::stringstream ss;
  std::string item;
  
  ss.str(str);
  while (getline(ss, item, delim)) {
    vec.push_back(item);
  }
}

void build_path(const std::string& str, std::vector < std::string >& path)
{
  // format: (,coupled_model)*:atomic_model.port 
  split(str, ',', path);
  if (path[0].empty()) {
    std::vector < std::string > path2;
    std::vector < std::string > path3;

    path.erase(path.begin());
    split(path[path.size() - 1], ':', path2);      
    split(path2[path2.size() - 1], '.', path3);
    path.pop_back();
    path.push_back(path2[0]);
    path.push_back(path3[0]);
    path.push_back(path3[1]);
  }
}

void push(Local < Object >& dic, const std::vector < std::string > path,
	  Local < Array >& value, Isolate* isolate)
{
  Local < Object > p = dic;
  unsigned int index = 0;
  
  while (p->HasOwnProperty(String::NewFromUtf8(isolate, path[index].c_str()))) {
    p = Local < Object>::Cast(p->Get(String::NewFromUtf8(isolate,
							 path[index].c_str())));
    ++index;
  }
  if (index < path.size() - 2) {
    for (unsigned int i = index; i < path.size() - 1; ++i) {
      Local < Object > entry = Object::New(isolate);
      
      p->Set(String::NewFromUtf8(isolate, path[i].c_str()), entry);
      p = entry;
    }
  }
  p->Set(String::NewFromUtf8(isolate, path.back().c_str()), value);
}

void build(Local < Object >& v, const value::Matrix& matrix,
	   Isolate* isolate)
{
  value::ConstMatrixView view(matrix.value());
  unsigned int nbcol = matrix.columns();
  unsigned int nbline = view.shape()[1];

  for(unsigned int c = 0; c < nbcol; c++){
    Local < Array > col = Array::New(isolate);
    value::ConstVectorView t = matrix.column(c);

    if (matrix.getString(c,0) == "time") {
      for (unsigned int i = 1; i < nbline; ++i) {
	col->Set(i - 1, convert_value(*t[i], isolate));
      }
      v->Set(String::NewFromUtf8(isolate, "time"), col);
    } else {
      std::vector < std::string > path;
  
      build_path(matrix.getString(c,0), path);
      for (unsigned int i = 1; i < nbline; ++i) {
	if (t[i]) {
	  col->Set(i - 1, convert_value(*t[i], isolate));
	} else {
	  col->Set(i - 1, Null(isolate));
	}
      }
      push(v, path, col, isolate);
    }
  }
}

void convert(const value::Map& out, Local < Object >& result, Isolate* isolate)
{
  for(value::Map::const_iterator itb = out.begin(); itb != out.end();
      ++itb) {
    Local < Object > view = Object::New(isolate);

    build(view, itb->second->toMatrix(), isolate);
    result->Set(String::NewFromUtf8(isolate, itb->first.c_str()), view);
  }
}

Persistent<Function> VleWrapper::constructor;

void VleWrapper::Init(v8::Handle<v8::Object> exports)
{
  Isolate* isolate = exports->GetIsolate();
  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
  
  tpl->SetClassName(String::NewFromUtf8(isolate, "Vle"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  NODE_SET_PROTOTYPE_METHOD(tpl, "get_duration", get_duration);
  NODE_SET_PROTOTYPE_METHOD(tpl, "run", run);

  constructor.Reset(isolate, tpl->GetFunction());
  exports->Set(String::NewFromUtf8(isolate, "Vle"),
	       tpl->GetFunction());
}

void VleWrapper::New(const v8::FunctionCallbackInfo<v8::Value>& jsargs)
{
  Isolate* isolate = jsargs.GetIsolate();
  
  if (jsargs.IsConstructCall()) {
    Local < String > arg0 = jsargs[0]->ToString();
    Local < String > arg1 = jsargs[1]->ToString();
    std::string pkgname = *String::Utf8Value(arg0);
    std::string filename = *String::Utf8Value(arg1);
    VleWrapper* obj = new VleWrapper(pkgname.c_str(), filename.c_str());
    
    obj->Wrap(jsargs.This());
    jsargs.GetReturnValue().Set(jsargs.This());
  } else {
    
  }
}

void VleWrapper::get_duration(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());

  args.GetReturnValue().Set(Number::New(isolate,
					obj->_vpz->project().experiment().duration()));
}

void VleWrapper::run(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
  value::Map* res = NULL;

  try {
    utils::ModuleManager man;
    manager::Error error;
    manager::Simulation sim(manager::LOG_NONE,
			    manager::SIMULATION_NONE,
			    NULL);
    //configure output plugins for column names
    vpz::Outputs::iterator itb =
      obj->_vpz->project().experiment().views().outputs().begin();
    vpz::Outputs::iterator ite =
      obj->_vpz->project().experiment().views().outputs().end();
    
    for(; itb!=ite; itb++) {
      vpz::Output& output = itb->second;
      
      if (output.package() == "vle.output" and
	  output.plugin() == "storage") {
	value::Map* configOutput = new value::Map();
	
	configOutput->addString("header", "top");
	output.setData(configOutput);
      }
    }

    res = sim.run(new vpz::Vpz(*obj->_vpz), man, &error);

    if (res == NULL) {
      args.GetReturnValue().Set(Null(isolate));
    } else {
      Local < Object > retval = Object::New(isolate);

      convert(*res, retval, isolate);
      args.GetReturnValue().Set(retval);
    }
  } catch(const std::exception& e) {
    args.GetReturnValue().Set(Null(isolate));
  }
}

void InitAll(Local<Object> exports) {
  VleWrapper::Init(exports);
}

NODE_MODULE(vle_node, InitAll)
