; ModuleID = '.\_scratch\split_print_verify.sere'
source_filename = ".\\_scratch\\split_print_verify.sere"

%Exception = type { ptr }

@0 = private unnamed_addr constant [18 x i8] c"Enter your name: \00", align 1
@1 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@2 = private unnamed_addr constant [6 x i8] c"Bye, \00", align 1
@3 = private unnamed_addr constant [2 x i8] c"!\00", align 1

define ptr @input(ptr %0) {
entry:
  unreachable
}

define i32 @abs(i32 %0) {
entry:
  %1 = icmp slt i32 %0, 0
  br i1 %1, label %if.body0, label %if.end

if.end:                                           ; preds = %entry
  ret i32 %0

if.body0:                                         ; preds = %entry
  %2 = sub i32 0, %0
  ret i32 %2
}

define i32 @min(i32 %0, i32 %1) {
entry:
  %2 = icmp slt i32 %0, %1
  br i1 %2, label %if.body0, label %if.body1

if.end:                                           ; No predecessors!
  unreachable

if.body0:                                         ; preds = %entry
  ret i32 %0

if.body1:                                         ; preds = %entry
  ret i32 %1
}

define i32 @max(i32 %0, i32 %1) {
entry:
  %2 = icmp sgt i32 %0, %1
  br i1 %2, label %if.body0, label %if.body1

if.end:                                           ; No predecessors!
  unreachable

if.body0:                                         ; preds = %entry
  ret i32 %0

if.body1:                                         ; preds = %entry
  ret i32 %1
}

define i32 @clamp(i32 %0, i32 %1, i32 %2) {
entry:
  %3 = call i32 @max(i32 %0, i32 %1)
  %4 = call i32 @min(i32 %3, i32 %2)
  ret i32 %4
}

define i32 @sign(i32 %0) {
entry:
  %1 = icmp sgt i32 %0, 0
  br i1 %1, label %if.body0, label %if.body1

if.end:                                           ; preds = %if.body1
  ret i32 0

if.body0:                                         ; preds = %entry
  ret i32 1

if.body1:                                         ; preds = %if.body1, %entry
  %2 = icmp slt i32 %0, 0
  br i1 %2, label %if.body1, label %if.end
}

define void @prelude___init__(%Exception %0, ptr %1) {
entry:
  ret void
}

define i32 @main() {
entry:
  %0 = alloca ptr, align 8
  %1 = alloca i64, align 8
  call void @sere_list_str_repr_data(ptr undef, ptr %0, ptr %1)
  %2 = load ptr, ptr %0, align 8
  %3 = call i64 @strlen(ptr %2)
  call void @sere_print_str(ptr %2, i64 %3)
  %4 = alloca ptr, align 8
  %5 = alloca i64, align 8
  %6 = call i64 @strlen(ptr @0)
  call void @sere_input(ptr @0, i64 %6, ptr %4, ptr %5)
  %7 = load ptr, ptr %4, align 8
  %8 = alloca ptr, align 8
  store ptr %7, ptr %8, align 8
  %9 = load ptr, ptr %8, align 8
  %10 = call i64 @strlen(ptr %9)
  %11 = call i64 @strlen(ptr @1)
  %12 = call ptr @sere_string_split(ptr %9, i64 %10, ptr @1, i64 %11)
  %13 = alloca ptr, align 8
  %14 = alloca i64, align 8
  call void @sere_list_str_repr_data(ptr %12, ptr %13, ptr %14)
  %15 = load ptr, ptr %13, align 8
  %16 = call i64 @strlen(ptr @2)
  %17 = call i64 @strlen(ptr %15)
  %18 = alloca i64, align 8
  %19 = call ptr @sere_str_concat_data(ptr @2, i64 %16, ptr %15, i64 %17, ptr %18)
  %20 = load i64, ptr %18, align 4
  %21 = call i64 @strlen(ptr @3)
  %22 = alloca i64, align 8
  %23 = call ptr @sere_str_concat_data(ptr %19, i64 %20, ptr @3, i64 %21, ptr %22)
  %24 = load i64, ptr %22, align 4
  %25 = call i64 @strlen(ptr %23)
  call void @sere_print_str(ptr %23, i64 %25)
  ret i32 0
}

declare void @sere_list_str_repr_data(ptr, ptr, ptr)

declare void @sere_print_str(ptr, i64)

declare i64 @strlen(ptr)

declare void @sere_input(ptr, i64, ptr, ptr)

declare ptr @sere_string_split(ptr, i64, ptr, i64)

declare ptr @sere_str_concat_data(ptr, i64, ptr, i64, ptr)
