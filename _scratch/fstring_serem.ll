; ModuleID = '.\_scratch\fstring_input.sere'
source_filename = ".\\_scratch\\fstring_input.sere"

%Exception = type { ptr }

@0 = private unnamed_addr constant [18 x i8] c"Enter your name: \00", align 1
@1 = private unnamed_addr constant [8 x i8] c"Hello, \00", align 1
@2 = private unnamed_addr constant [2 x i8] c"!\00", align 1

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
  %0 = call ptr @input(ptr @0)
  %1 = alloca ptr, align 8
  store ptr %0, ptr %1, align 8
  %2 = load ptr, ptr %1, align 8
  %3 = call i64 @strlen(ptr @1)
  %4 = call i64 @strlen(ptr %2)
  %5 = alloca i64, align 8
  %6 = call ptr @sere_str_concat_data(ptr @1, i64 %3, ptr %2, i64 %4, ptr %5)
  %7 = load i64, ptr %5, align 4
  %8 = call i64 @strlen(ptr @2)
  %9 = alloca i64, align 8
  %10 = call ptr @sere_str_concat_data(ptr %6, i64 %7, ptr @2, i64 %8, ptr %9)
  %11 = load i64, ptr %9, align 4
  %12 = call i64 @strlen(ptr %10)
  call void @sere_print_str(ptr %10, i64 %12)
  ret i32 0
}

declare ptr @sere_str_concat_data(ptr, i64, ptr, i64, ptr)

declare i64 @strlen(ptr)

declare void @sere_print_str(ptr, i64)
