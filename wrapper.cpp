#include <vle/vle.hpp>
#include <vle/manager/Manager.hpp>
#include <vle/manager/Simulation.hpp>
#include <vle/vpz/Vpz.hpp>
#include <vle/utils/Package.hpp>

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

#include <iostream>
#include <sstream>

using namespace vle;
using namespace v8;

static bool thread_init = false;

class VleWrapper : public node::ObjectWrap
{
public:
  static void Init(Handle < Object > exports);
  
private:
  vpz::Vpz* _vpz;

  explicit VleWrapper(const char* pkg_name, const char* file_name)
  {
    try {
      if (!thread_init) {
	vle::Init app;
	
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

  static Persistent<Function> constructor;

  static void New(const FunctionCallbackInfo<Value>& args);  

  static void experiment_set_begin(const FunctionCallbackInfo<Value>& args);
  static void experiment_get_begin(const FunctionCallbackInfo<Value>& args);
  static void experiment_set_duration(const FunctionCallbackInfo<Value>& args);
  static void experiment_get_duration(const FunctionCallbackInfo<Value>& args);
  static void experiment_set_seed(const FunctionCallbackInfo<Value>& args);
  static void experiment_get_seed(const FunctionCallbackInfo<Value>& args);

  static void run(const FunctionCallbackInfo<Value>& args);
  static void run_manager(const FunctionCallbackInfo<Value>& args);
  static void run_manager_thread(const FunctionCallbackInfo<Value>& args);

  static void condition_list(const FunctionCallbackInfo<Value>& args);
  static void condition_show(const FunctionCallbackInfo<Value>& args);
  static void condition_create(const FunctionCallbackInfo<Value>& args);
  static void condition_port_list(const FunctionCallbackInfo<Value>& args);
  static void condition_port_clear(const FunctionCallbackInfo<Value>& args);
  static void condition_add_real(const FunctionCallbackInfo<Value>& args);
  static void condition_add_integer(const FunctionCallbackInfo<Value>& args);
  static void condition_add_string(const FunctionCallbackInfo<Value>& args);
  static void condition_add_boolean(const FunctionCallbackInfo<Value>& args);
  static void condition_add_value(const FunctionCallbackInfo<Value>& args);
  static void condition_set_port_value(const FunctionCallbackInfo<Value>& args);
  static void condition_get_setvalue(const FunctionCallbackInfo<Value>& args);
  static void condition_get_value(const FunctionCallbackInfo<Value>& args);
  static void condition_get_value_type(const FunctionCallbackInfo<Value>& args);
  static void condition_delete_value(const FunctionCallbackInfo<Value>& args);

  static void output_set_plugin(const FunctionCallbackInfo<Value>& args);
  static void outputs_list(const FunctionCallbackInfo<Value>& args);
};

class ValueWrapper : public node::ObjectWrap
{
public:
  static void Init(Handle < Object > exports);
  
  const value::Value* get_value() const
  { return _value; }

private:
  value::Value* _value;

  explicit ValueWrapper() : _value(0)
  { }

  virtual ~ValueWrapper()
  { if (_value) delete _value; }

  static Persistent<Function> constructor;
  
  static void New(const FunctionCallbackInfo<Value>& args);  

  static void get_type(const FunctionCallbackInfo<Value>& args);
};

/*  - - - - - - - - - - - - - --ooOoo-- - - - - - - - - - - -  */

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

void convert_list(const value::Matrix& out, Local < Array >& result,
		  Isolate* isolate)
{
  for (unsigned int j = 0; j < out.columns(); j++) {
    Local < Array > line = Array::New(isolate);
    
    for (unsigned int i = 0; i < out.column(0).size(); i++) {
      Local < Object > item = Object::New(isolate);

      convert(out.get(j,i)->toMap(), item , isolate);
    line->Set(i, item);
    }
    result->Set(j, line);
  }
}

/*  - - - - - - - - - - - - - --ooOoo-- - - - - - - - - - - -  */

Persistent<Function> ValueWrapper::constructor;

void ValueWrapper::Init(Handle<Object> exports)
{
  Isolate* isolate = exports->GetIsolate();
  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
  
  tpl->SetClassName(String::NewFromUtf8(isolate, "Value"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  NODE_SET_PROTOTYPE_METHOD(tpl, "get_type", get_type);

  constructor.Reset(isolate, tpl->GetFunction());
  exports->Set(String::NewFromUtf8(isolate, "Value"),
	       tpl->GetFunction());
}

value::Value* convert_to_vle(Local < Value > v)
{
    if (v->IsNumber()) {
      Local < Number > arg = v->ToNumber();
      double value = arg->Value();

      if (value == static_cast < int >(value)) {
	return value::Integer::create(static_cast < int >(value));
      } else {
	return value::Double::create(value);	
      }
    } else if (v->IsString()) {
      Local < String > arg = v->ToString();
      std::string value = *String::Utf8Value(arg);

      return value::String::create(value);	
    } else if (v->IsBoolean()) {
      Local < Boolean > arg = v->ToBoolean();
      bool value = arg->Value();

      return value::Boolean::create(value);	
    } else if (v->IsArray()) {
      Local < Array > arg = Local < Array >::Cast(v);
      value::Set* result = value::Set::create();
      
      for (unsigned int i = 0; i < arg->Length(); ++i) {
	result->add(convert_to_vle(arg->Get(i)));
      }
      return result;
    } else if (v->IsObject()) {
      Local < Object > arg = Local < Object >::Cast(v);
      Local < Array > properties = arg->GetOwnPropertyNames();
      value::Map* result = value::Map::create();
      
      for (unsigned int i = 0; i < properties->Length(); ++i) {
	Local < Value > key = properties->Get(i);
	
	result->add(*String::Utf8Value(key),
		    convert_to_vle(arg->Get(key)));
      }
      return result;
    } else {
      return 0;
    }
}

void ValueWrapper::New(const FunctionCallbackInfo<Value>& jsargs)
{
  Isolate* isolate = jsargs.GetIsolate();
  
  if (jsargs.IsConstructCall()) {
    ValueWrapper* obj = new ValueWrapper();

    obj->_value = convert_to_vle(jsargs[0]);
    obj->Wrap(jsargs.This());
    jsargs.GetReturnValue().Set(jsargs.This());
  } else {
    
  }
}

/*  - - - - - - - - - - - - - --ooOoo-- - - - - - - - - - - -  */

Persistent<Function> VleWrapper::constructor;

void VleWrapper::Init(Handle<Object> exports)
{
  Isolate* isolate = exports->GetIsolate();
  Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
  
  tpl->SetClassName(String::NewFromUtf8(isolate, "Vle"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  NODE_SET_PROTOTYPE_METHOD(tpl, "get_begin", experiment_get_begin);
  NODE_SET_PROTOTYPE_METHOD(tpl, "set_begin", experiment_set_begin);
  NODE_SET_PROTOTYPE_METHOD(tpl, "get_duration", experiment_get_duration);
  NODE_SET_PROTOTYPE_METHOD(tpl, "set_duration", experiment_set_duration);
  NODE_SET_PROTOTYPE_METHOD(tpl, "get_seed", experiment_get_seed);
  NODE_SET_PROTOTYPE_METHOD(tpl, "set_seed", experiment_set_seed);
  NODE_SET_PROTOTYPE_METHOD(tpl, "run", run);
  NODE_SET_PROTOTYPE_METHOD(tpl, "run_manager", run_manager);
  NODE_SET_PROTOTYPE_METHOD(tpl, "run_manager_thread", run_manager_thread);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_list", condition_list);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_show", condition_show);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_create", condition_create);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_port_list", condition_port_list);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_port_clear", condition_port_clear);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_add_real", condition_add_real);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_add_integer",
			    condition_add_integer);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_add_string", condition_add_string);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_add_boolean",
			    condition_add_boolean);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_add_value", condition_add_value);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_set_port_value",
			    condition_set_port_value);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_get_setvalue",
			    condition_get_setvalue);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_get_value", condition_get_value);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_get_value_type",
			    condition_get_value_type);
  NODE_SET_PROTOTYPE_METHOD(tpl, "condition_delete_value",
			    condition_delete_value);
  NODE_SET_PROTOTYPE_METHOD(tpl, "output_set_plugin", output_set_plugin);
  NODE_SET_PROTOTYPE_METHOD(tpl, "outputs_list", outputs_list);
  
  constructor.Reset(isolate, tpl->GetFunction());
  exports->Set(String::NewFromUtf8(isolate, "Vle"),
	       tpl->GetFunction());
}

void VleWrapper::New(const FunctionCallbackInfo<Value>& jsargs)
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

/*  - - - - - - - - - - - - - --ooOoo-- - - - - - - - - - - -  */

void ValueWrapper::get_type(const FunctionCallbackInfo<Value>& args)
{
  Isolate* isolate = args.GetIsolate();
  ValueWrapper* obj = ObjectWrap::Unwrap<ValueWrapper>(args.Holder());

  assert(obj->_value == 0);
  
  switch(obj->_value->getType()) {
  case vle::value::Value::DOUBLE: {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "double"));
  }
  case vle::value::Value::INTEGER: {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "integer"));
  }
  case vle::value::Value::STRING: {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "string"));
  }
  case vle::value::Value::BOOLEAN: {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "boolean"));
  }
  case vle::value::Value::MAP: {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "map"));
  }
  case vle::value::Value::SET: {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "set"));
  }
  case vle::value::Value::TUPLE : {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "typle"));
  }
  case vle::value::Value::TABLE : {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "table"));
  }
  case vle::value::Value::XMLTYPE : {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "xml"));
  }
  case vle::value::Value::MATRIX : {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "matrix"));
  }
  case vle::value::Value::NIL : {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "none"));
  }
  default : {
    args.GetReturnValue().Set(String::NewFromUtf8(isolate, "none"));
  }
  }
}

/*  - - - - - - - - - - - - - --ooOoo-- - - - - - - - - - - -  */

void VleWrapper::experiment_set_begin(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 0) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < Number > arg0 = args[0]->ToNumber();
  
    obj->_vpz->project().experiment().setBegin(arg0->Value());
  }
}

void VleWrapper::experiment_get_begin(const FunctionCallbackInfo<Value>& args)
{
  Isolate* isolate = args.GetIsolate();
  VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
  
  args.GetReturnValue().Set(Number::New(isolate,
					obj->_vpz->project().experiment().
					begin()));
}

void VleWrapper::experiment_set_duration(const FunctionCallbackInfo<Value>&
					 args)
{
  if (args.Length() > 0) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < Number > arg0 = args[0]->ToNumber();
    
    obj->_vpz->project().experiment().setDuration(arg0->Value());
  }
}

void VleWrapper::experiment_get_duration(const FunctionCallbackInfo<Value>&
					 args)
{
  Isolate* isolate = args.GetIsolate();
  VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
  
  args.GetReturnValue().Set(Number::New(isolate,
					obj->_vpz->project().experiment().
					duration()));
}

void VleWrapper::experiment_set_seed(const FunctionCallbackInfo<Value>& args)
{
  //TODO
}

void VleWrapper::experiment_get_seed(const FunctionCallbackInfo<Value>& args)
{
  // TODO
}

void VleWrapper::run(const FunctionCallbackInfo<Value>& args)
{
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
    for(vpz::Outputs::iterator it =
	  obj->_vpz->project().experiment().views().outputs().begin();
	it != obj->_vpz->project().experiment().views().outputs().end(); ++it) {
      vpz::Output& output = it->second;
      
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
      delete res;
      args.GetReturnValue().Set(retval);
    }
  } catch(const std::exception& e) {
    args.GetReturnValue().Set(Null(isolate));
  }
}

void VleWrapper::run_manager(const FunctionCallbackInfo<Value>& args)
{
  Isolate* isolate = args.GetIsolate();

  VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
  value::Matrix* res = NULL;

  try {
    utils::ModuleManager man;
    manager::Error error;
    manager::Manager sim(manager::LOG_NONE,
			 manager::SIMULATION_NONE,
			 NULL);
    
    //configure output plugins for column names
    for(vpz::Outputs::iterator it =
	  obj->_vpz->project().experiment().views().outputs().begin();
	it != obj->_vpz->project().experiment().views().outputs().end(); ++it) {
      vpz::Output& output = it->second;
      
      if (output.package() == "vle.output" and
	  output.plugin() == "storage") {
	value::Map* configOutput = new value::Map();
	
	configOutput->addString("header", "top");
	output.setData(configOutput);
      }
    }

    res = sim.run(new vpz::Vpz(*obj->_vpz), man, 1, 0, 1, &error);

    if (res == NULL) {
      args.GetReturnValue().Set(Null(isolate));
    } else {
      Local < Array > retval = Array::New(isolate);

      convert_list(*res, retval, isolate);
      delete res;
      args.GetReturnValue().Set(retval);
    }
  } catch(const std::exception& e) {
    args.GetReturnValue().Set(Null(isolate));
  }
}

void VleWrapper::run_manager_thread(const FunctionCallbackInfo<Value>& args)
{
  Local < Number > arg0 = args[0]->ToNumber();
  Isolate* isolate = args.GetIsolate();

  VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
  value::Matrix* res = NULL;

  try {
    utils::ModuleManager man;
    manager::Error error;
    manager::Manager sim(manager::LOG_NONE,
			 manager::SIMULATION_NONE,
			 NULL);
    
    //configure output plugins for column names
    for(vpz::Outputs::iterator it =
	  obj->_vpz->project().experiment().views().outputs().begin();
	it != obj->_vpz->project().experiment().views().outputs().end(); ++it) {
      vpz::Output& output = it->second;
      
      if (output.package() == "vle.output" and
	  output.plugin() == "storage") {
	value::Map* configOutput = new value::Map();
	
	configOutput->addString("header", "top");
	output.setData(configOutput);
      }
    }

    res = sim.run(new vpz::Vpz(*obj->_vpz), man,
		  static_cast < int >(arg0->Value()), 0, 1, &error);

    if (res == NULL) {
      args.GetReturnValue().Set(Null(isolate));
    } else {
      Local < Array > retval = Array::New(isolate);

      convert_list(*res, retval, isolate);
      delete res;
      args.GetReturnValue().Set(retval);
    }
  } catch(const std::exception& e) {
    args.GetReturnValue().Set(Null(isolate));
  }
}

void VleWrapper::condition_list(const FunctionCallbackInfo<Value>& args)
{
  Isolate* isolate = args.GetIsolate();
  VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
  Local < Array > result = Array::New(isolate);
  int size;

  size = obj->_vpz->project().experiment().conditions().conditionlist().size();
  if (size > 0) {
    std::list < std::string > lst;
    std::list < std::string >::const_iterator it;
    int i;

    obj->_vpz->project().experiment().conditions().conditionnames(lst);
    i = 0;
    for (it = lst.begin(); it != lst.end(); ++it, ++i)
      result->Set(i, String::NewFromUtf8(isolate, it->c_str()));
  }
  args.GetReturnValue().Set(result);
}

void VleWrapper::condition_show(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 1) {
    Isolate* isolate = args.GetIsolate();
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    vle::vpz::Condition& cnd(obj->_vpz->project().experiment().
			     conditions().get(conditionname));
    vle::value::VectorValue& v(cnd.getSetValues(portname).value());
    int size = v.size();

    if (size > 1) {
      Local < Array > result = Array::New(isolate);
    
      for (int i = 0; i < size; ++i)
	result->Set(i, convert_value(*v[i], isolate));
      args.GetReturnValue().Set(result);
    } else if (size == 1) {
      args.GetReturnValue().Set(convert_value(*v[0], isolate));
    } else {
      args.GetReturnValue().Set(Null(isolate));
    }
  }
}

void VleWrapper::condition_create(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 0) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    std::string name = *String::Utf8Value(arg0);
    vpz::Condition newCond(name);
    vpz::Conditions& listConditions(obj->_vpz->project().experiment().
				    conditions());

    listConditions.add(newCond);
  }
}

void VleWrapper::condition_port_list(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 0) {
    Isolate* isolate = args.GetIsolate();
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    std::string conditionname = *String::Utf8Value(arg0);
    Local < Array > result = Array::New(isolate);
    const vpz::Condition& cnd(obj->_vpz->project().experiment().conditions().
			      get(conditionname));

    if (cnd.conditionvalues().size() > 0) {
      std::list < std::string > lst;
      std::list < std::string >::const_iterator it;
      int i = 0;

      cnd.portnames(lst);
      for (it = lst.begin(); it != lst.end(); ++it, ++i)
	result->Set(i, String::NewFromUtf8(isolate, it->c_str()));
    }
    args.GetReturnValue().Set(result);
  }
}

void VleWrapper::condition_port_clear(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 1) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));

    cnd.clearValueOfPort(portname);
  }
}

void VleWrapper::condition_add_real(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 2) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < Number > arg2 = args[2]->ToNumber();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    double value = arg2->Value();
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));
  
    cnd.addValueToPort(portname, value::Double::create(value));
  }
}

void VleWrapper::condition_add_integer(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 2) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < Number > arg2 = args[2]->ToNumber();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    long value = static_cast < long >(arg2->Value());
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));
  
    cnd.addValueToPort(portname, value::Integer::create(value));
  }
}

void VleWrapper::condition_add_string(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 2) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < String > arg2 = args[2]->ToString();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    std::string value = *String::Utf8Value(arg2);
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));
  
    cnd.addValueToPort(portname, value::String::create(value));
  }
}

void VleWrapper::condition_add_boolean(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 2) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < Boolean > arg2 = args[2]->ToBoolean();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    bool value = arg2->Value();
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));
  
    cnd.addValueToPort(portname, value::Boolean::create(value));
  }
}

void VleWrapper::condition_add_value(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 2) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < Object > arg2 = args[2]->ToObject();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    ValueWrapper* value = ObjectWrap::Unwrap<ValueWrapper>(arg2);
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));

    cnd.addValueToPort(portname, *value->get_value());
  }
}

void VleWrapper::condition_set_port_value(const FunctionCallbackInfo<Value>&
					  args)
{
  if (args.Length() > 3) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < Object > arg2 = args[2]->ToObject();
    Local < Number > arg3 = args[3]->ToNumber();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    ValueWrapper* value = ObjectWrap::Unwrap<ValueWrapper>(arg2);
    unsigned int index = static_cast < unsigned int >(arg3->Value());

    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));
    value::VectorValue& vector(cnd.getSetValues(portname).value());

    vector.at(index) = value->get_value()->clone();
  }
}

void VleWrapper::condition_get_setvalue(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 1) {
    Isolate* isolate = args.GetIsolate();
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));
    value::VectorValue& v(cnd.getSetValues(portname).value());
    Local < Array > result = Array::New(isolate);

    for (unsigned int i = 0; i < v.size(); ++i) {
      result->Set(i, convert_value(*v[i], isolate));
    }
    args.GetReturnValue().Set(result);
  }
}

void VleWrapper::condition_get_value(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 2) {
    Isolate* isolate = args.GetIsolate();
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < Number > arg2 = args[2]->ToNumber();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    unsigned int index = static_cast < unsigned int >(arg2->Value());
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));
    value::VectorValue& v(cnd.getSetValues(portname).value());
  
    args.GetReturnValue().Set(convert_value(*v[index], isolate));
  }
}

void VleWrapper::condition_get_value_type(const FunctionCallbackInfo<Value>&
					  args)
{
  if (args.Length() > 2) {
    Isolate* isolate = args.GetIsolate();
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < Number > arg2 = args[2]->ToNumber();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    unsigned int index = static_cast < unsigned int >(arg2->Value());
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));
    value::VectorValue& v(cnd.getSetValues(portname).value());

    switch(v[index]->getType()) {
    case vle::value::Value::DOUBLE: {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "double"));
    }
    case vle::value::Value::INTEGER: {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "integer"));
    }
    case vle::value::Value::STRING: {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "string"));
    }
    case vle::value::Value::BOOLEAN: {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "boolean"));
    }
    case vle::value::Value::MAP: {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "map"));
    }
    case vle::value::Value::SET: {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "set"));
    }
    case vle::value::Value::TUPLE : {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "typle"));
    }
    case vle::value::Value::TABLE : {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "table"));
    }
    case vle::value::Value::XMLTYPE : {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "xml"));
    }
    case vle::value::Value::MATRIX : {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "matrix"));
    }
    case vle::value::Value::NIL : {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "none"));
    }
    default : {
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "none"));
    }
    }
  }
}

void VleWrapper::condition_delete_value(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 2) {  
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < Number > arg2 = args[2]->ToNumber();
    std::string conditionname = *String::Utf8Value(arg0);
    std::string portname = *String::Utf8Value(arg1);
    unsigned int index = static_cast < unsigned int >(arg2->Value());  
    vpz::Condition& cnd(obj->_vpz->project().experiment().
			conditions().get(conditionname));
    vle::value::VectorValue& vector(cnd.getSetValues(portname).value());
    vle::value::VectorValue::iterator it = vector.begin();
    value::Value* base = vector[index];

    while (it != vector.end()) {
      if (&**it == base)
	break;
      ++it;
    }
    if (it != vector.end()) {
      vector.erase(it);
    }
  }
}

void VleWrapper::output_set_plugin(const FunctionCallbackInfo<Value>& args)
{
  if (args.Length() > 4) {
    VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
    Local < String > arg0 = args[0]->ToString();
    Local < String > arg1 = args[1]->ToString();
    Local < String > arg2 = args[2]->ToString();
    Local < String > arg3 = args[3]->ToString();
    Local < String > arg4 = args[4]->ToString();
    std::string outputname = *String::Utf8Value(arg0);
    std::string location = *String::Utf8Value(arg1);
    std::string format = *String::Utf8Value(arg2);
    std::string plugin = *String::Utf8Value(arg3);
    std::string package = *String::Utf8Value(arg4);
  
    vpz::Output& out(obj->_vpz->project().experiment().views().outputs().
		     get(outputname));

    if (format == "local") {
      out.setLocalStream(location, plugin, package);
    } else {
      out.setDistantStream(location, plugin, package);
    }
  }
}

void VleWrapper::outputs_list(const FunctionCallbackInfo<Value>& args)
{
  Isolate* isolate = args.GetIsolate();
  VleWrapper* obj = ObjectWrap::Unwrap<VleWrapper>(args.Holder());
  vpz::OutputList& lst(obj->_vpz->project().experiment().views().outputs().
		       outputlist());
  Local < Array > result = Array::New(isolate);
  int i = 0;
  
  for (vpz::OutputList::const_iterator it = lst.begin(); it != lst.end();
       ++it, ++i) {
    result->Set(i, String::NewFromUtf8(isolate, it->first.c_str()));
  }
  args.GetReturnValue().Set(result);
}

/*  - - - - - - - - - - - - - --ooOoo-- - - - - - - - - - - -  */

void InitAll(Local<Object> exports) {
  ValueWrapper::Init(exports);
  VleWrapper::Init(exports);
}

NODE_MODULE(vle_node, InitAll)
