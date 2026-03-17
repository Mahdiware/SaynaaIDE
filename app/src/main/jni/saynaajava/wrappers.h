#pragma once

void* new_java_class_instance(VM* vm);
void delete_java_class_instance(VM* vm, void* ptr);
void* new_java_object_instance(VM* vm);
void delete_java_object_instance(VM* vm, void* ptr);
void* new_java_method_instance(VM* vm);
void delete_java_method_instance(VM* vm, void* ptr);

void java_class_init(VM* vm);
void java_object_init(VM* vm);
void java_method_init(VM* vm);

void java_class_getter(VM* vm);
void java_class_call(VM* vm);
void java_object_getter(VM* vm);
void java_object_setter(VM* vm);
void java_method_call(VM* vm);

void java_class_str(VM* vm);
void java_object_str(VM* vm);
void java_method_str(VM* vm);

void fn_activity(VM* vm);
void fn_eventView(VM* vm);
