; ModuleID = '_scratch\op_overload.sere'
source_filename = "_scratch\\op_overload.sere"
target triple = "x86_64-pc-windows-msvc"

%Exception = type { i32, { ptr, i64 } }
%Vec = type { i32, i32, i32 }
%Money = type { i32, i32 }
%Object = type { i32, { ptr, i64 } }

@sere.module.init.done = internal global i8 0
@0 = private unnamed_addr constant [5 x i8] c"Vec(\00", align 1
@1 = private unnamed_addr constant [3 x i8] c", \00", align 1
@2 = private unnamed_addr constant [2 x i8] c")\00", align 1
@3 = private unnamed_addr constant [2 x i8] c"c\00", align 1
@4 = private unnamed_addr constant [6 x i8] c"Num: \00", align 1
@5 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@6 = private unnamed_addr constant [11 x i8] c"Num: Hello\00", align 1
@7 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@8 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@9 = private unnamed_addr constant [3 x i8] c"! \00", align 1
@10 = private unnamed_addr constant [14 x i8] c"Num: Hello! 5\00", align 1
@11 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@12 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@13 = private unnamed_addr constant [2 x i8] c" \00", align 1
@14 = private unnamed_addr constant [2 x i8] c" \00", align 1
@15 = private unnamed_addr constant [3 x i8] c": \00", align 1
@16 = private unnamed_addr constant [3 x i8] c": \00", align 1
@17 = private unnamed_addr constant [4 x i8] c"end\00", align 1
@18 = private unnamed_addr constant [2 x i8] c" \00", align 1
@19 = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@20 = private unnamed_addr constant [26 x i8] c"HelloHelloHelloHelloHello\00", align 1
@21 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@22 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@23 = private unnamed_addr constant [3 x i8] c"ab\00", align 1
@24 = private unnamed_addr constant [7 x i8] c"ababab\00", align 1
@25 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@26 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@27 = private unnamed_addr constant [2 x i8] c" \00", align 1
@28 = private unnamed_addr constant [3 x i8] c"Hi\00", align 1
@29 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@30 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@31 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@32 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@33 = private unnamed_addr constant [2 x i8] c" \00", align 1
@34 = private unnamed_addr constant [2 x i8] c"[\00", align 1
@35 = private unnamed_addr constant [3 x i8] c", \00", align 1
@36 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@37 = private unnamed_addr constant [2 x i8] c"]\00", align 1
@38 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@39 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@40 = private unnamed_addr constant [2 x i8] c" \00", align 1
@41 = private unnamed_addr constant [2 x i8] c" \00", align 1
@42 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@43 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@44 = private unnamed_addr constant [2 x i8] c" \00", align 1
@45 = private unnamed_addr constant [2 x i8] c" \00", align 1
@46 = private unnamed_addr constant [2 x i8] c" \00", align 1
@47 = private unnamed_addr constant [2 x i8] c" \00", align 1
@48 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@49 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@50 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@51 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@52 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@53 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@54 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@55 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@56 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@57 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@58 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@59 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@60 = private unnamed_addr constant [2 x i8] c" \00", align 1
@61 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@62 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@63 = private unnamed_addr constant [2 x i8] c" \00", align 1
@64 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@65 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@66 = private unnamed_addr constant [7 x i8] c"Hello \00", align 1
@67 = private unnamed_addr constant [6 x i8] c"World\00", align 1
@68 = private unnamed_addr constant [2 x i8] c" \00", align 1
@69 = private unnamed_addr constant [12 x i8] c"Hello World\00", align 1
@70 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@71 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1
@72 = private unnamed_addr constant [17 x i8] c"assertion failed\00", align 1
@73 = private unnamed_addr constant [25 x i8] c"AssertionError;Exception\00", align 1

declare void @sere_input(ptr, i64, ptr, ptr)

define i32 @abs(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = icmp slt i32 %0, 0
  br i1 %1, label %if.then, label %if.next

if.end:                                           ; preds = %if.next
  %2 = load i32, ptr %value1, align 4
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err2, label %err.ok3

if.then:                                          ; preds = %entry
  %5 = load i32, ptr %value1, align 4
  %6 = sub i32 0, %5
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

if.next:                                          ; preds = %entry
  br label %if.end

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 %6

err2:                                             ; preds = %if.end
  ret i32 0

err.ok3:                                          ; preds = %if.end
  ret i32 %2
}

define i32 @min(i32 %left, i32 %right) {
entry:
  %left1 = alloca i32, align 4
  store i32 %left, ptr %left1, align 4
  %right2 = alloca i32, align 4
  store i32 %right, ptr %right2, align 4
  %0 = load i32, ptr %left1, align 4
  %1 = load i32, ptr %right2, align 4
  %2 = icmp slt i32 %0, %1
  br i1 %2, label %if.then, label %if.next

if.then:                                          ; preds = %entry
  %3 = load i32, ptr %left1, align 4
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  %6 = load i32, ptr %right2, align 4
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err3, label %err.ok4

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 %3

err3:                                             ; preds = %if.next
  ret i32 0

err.ok4:                                          ; preds = %if.next
  ret i32 %6
}

define i32 @max(i32 %left, i32 %right) {
entry:
  %left1 = alloca i32, align 4
  store i32 %left, ptr %left1, align 4
  %right2 = alloca i32, align 4
  store i32 %right, ptr %right2, align 4
  %0 = load i32, ptr %left1, align 4
  %1 = load i32, ptr %right2, align 4
  %2 = icmp sgt i32 %0, %1
  br i1 %2, label %if.then, label %if.next

if.then:                                          ; preds = %entry
  %3 = load i32, ptr %left1, align 4
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  %6 = load i32, ptr %right2, align 4
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err3, label %err.ok4

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 %3

err3:                                             ; preds = %if.next
  ret i32 0

err.ok4:                                          ; preds = %if.next
  ret i32 %6
}

define i32 @clamp(i32 %value, i32 %low, i32 %high) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %low2 = alloca i32, align 4
  store i32 %low, ptr %low2, align 4
  %high3 = alloca i32, align 4
  store i32 %high, ptr %high3, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = load i32, ptr %low2, align 4
  %2 = call i32 @max(i32 %0, i32 %1)
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  %5 = load i32, ptr %high3, align 4
  %6 = call i32 @min(i32 %2, i32 %5)
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err4, label %err.ok5

err4:                                             ; preds = %err.ok
  ret i32 0

err.ok5:                                          ; preds = %err.ok
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err6, label %err.ok7

err6:                                             ; preds = %err.ok5
  ret i32 0

err.ok7:                                          ; preds = %err.ok5
  ret i32 %6
}

define i32 @sign(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = icmp sgt i32 %0, 0
  br i1 %1, label %if.then, label %if.next

if.end:                                           ; preds = %if.next3
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err6, label %err.ok7

if.then:                                          ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

if.next:                                          ; preds = %entry
  %6 = load i32, ptr %value1, align 4
  %7 = icmp slt i32 %6, 0
  br i1 %7, label %if.then2, label %if.next3

err:                                              ; preds = %if.then
  ret i32 0

err.ok:                                           ; preds = %if.then
  ret i32 1

if.then2:                                         ; preds = %if.next
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err4, label %err.ok5

if.next3:                                         ; preds = %if.next
  br label %if.end

err4:                                             ; preds = %if.then2
  ret i32 0

err.ok5:                                          ; preds = %if.then2
  ret i32 -1

err6:                                             ; preds = %if.end
  ret i32 0

err.ok7:                                          ; preds = %if.end
  ret i32 0
}

define void @prelude_Exception___init__(ptr %self, { ptr, i64 } %message) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %message1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %message, ptr %message1, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Exception, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %message1, align 8
  store { ptr, i64 } %2, ptr %1, align 8
  ret void
}

define void @Vec___init__(ptr %self, i32 %x, i32 %y) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %y2 = alloca i32, align 4
  store i32 %y, ptr %y2, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Vec, ptr %0, i32 0, i32 1
  %2 = load i32, ptr %x1, align 4
  store i32 %2, ptr %1, align 4
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %Vec, ptr %3, i32 0, i32 2
  %5 = load i32, ptr %y2, align 4
  store i32 %5, ptr %4, align 4
  ret void
}

define %Vec @Vec___add__(ptr %self, %Vec %other) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %other1 = alloca %Vec, align 8
  store %Vec %other, ptr %other1, align 4
  %init.tmp = alloca %Vec, align 8
  store %Vec zeroinitializer, ptr %init.tmp, align 4
  %0 = getelementptr inbounds nuw %Vec, ptr %init.tmp, i32 0, i32 0
  store i32 -709171067, ptr %0, align 4
  %1 = load ptr, ptr %self.slot, align 8
  %2 = getelementptr inbounds nuw %Vec, ptr %1, i32 0, i32 1
  %3 = load i32, ptr %2, align 4
  %4 = getelementptr inbounds nuw %Vec, ptr %other1, i32 0, i32 1
  %5 = load i32, ptr %4, align 4
  %6 = add i32 %3, %5
  %7 = load ptr, ptr %self.slot, align 8
  %8 = getelementptr inbounds nuw %Vec, ptr %7, i32 0, i32 2
  %9 = load i32, ptr %8, align 4
  %10 = getelementptr inbounds nuw %Vec, ptr %other1, i32 0, i32 2
  %11 = load i32, ptr %10, align 4
  %12 = add i32 %9, %11
  call void @Vec___init__(ptr %init.tmp, i32 %6, i32 %12)
  %13 = load %Vec, ptr %init.tmp, align 4
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Vec zeroinitializer

err.ok:                                           ; preds = %entry
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Vec zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Vec %13
}

define %Vec @Vec___sub__(ptr %self, %Vec %other) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %other1 = alloca %Vec, align 8
  store %Vec %other, ptr %other1, align 4
  %init.tmp = alloca %Vec, align 8
  store %Vec zeroinitializer, ptr %init.tmp, align 4
  %0 = getelementptr inbounds nuw %Vec, ptr %init.tmp, i32 0, i32 0
  store i32 -709171067, ptr %0, align 4
  %1 = load ptr, ptr %self.slot, align 8
  %2 = getelementptr inbounds nuw %Vec, ptr %1, i32 0, i32 1
  %3 = load i32, ptr %2, align 4
  %4 = getelementptr inbounds nuw %Vec, ptr %other1, i32 0, i32 1
  %5 = load i32, ptr %4, align 4
  %6 = sub i32 %3, %5
  %7 = load ptr, ptr %self.slot, align 8
  %8 = getelementptr inbounds nuw %Vec, ptr %7, i32 0, i32 2
  %9 = load i32, ptr %8, align 4
  %10 = getelementptr inbounds nuw %Vec, ptr %other1, i32 0, i32 2
  %11 = load i32, ptr %10, align 4
  %12 = sub i32 %9, %11
  call void @Vec___init__(ptr %init.tmp, i32 %6, i32 %12)
  %13 = load %Vec, ptr %init.tmp, align 4
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Vec zeroinitializer

err.ok:                                           ; preds = %entry
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Vec zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Vec %13
}

define %Vec @Vec___mul__(ptr %self, i32 %factor) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %factor1 = alloca i32, align 4
  store i32 %factor, ptr %factor1, align 4
  %init.tmp = alloca %Vec, align 8
  store %Vec zeroinitializer, ptr %init.tmp, align 4
  %0 = getelementptr inbounds nuw %Vec, ptr %init.tmp, i32 0, i32 0
  store i32 -709171067, ptr %0, align 4
  %1 = load ptr, ptr %self.slot, align 8
  %2 = getelementptr inbounds nuw %Vec, ptr %1, i32 0, i32 1
  %3 = load i32, ptr %2, align 4
  %4 = load i32, ptr %factor1, align 4
  %5 = mul i32 %3, %4
  %6 = load ptr, ptr %self.slot, align 8
  %7 = getelementptr inbounds nuw %Vec, ptr %6, i32 0, i32 2
  %8 = load i32, ptr %7, align 4
  %9 = load i32, ptr %factor1, align 4
  %10 = mul i32 %8, %9
  call void @Vec___init__(ptr %init.tmp, i32 %5, i32 %10)
  %11 = load %Vec, ptr %init.tmp, align 4
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Vec zeroinitializer

err.ok:                                           ; preds = %entry
  %14 = call i32 @sere_has_error()
  %15 = icmp ne i32 %14, 0
  br i1 %15, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Vec zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Vec %11
}

define i1 @Vec___eq__(ptr %self, %Vec %other) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %other1 = alloca %Vec, align 8
  store %Vec %other, ptr %other1, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Vec, ptr %0, i32 0, i32 1
  %2 = load i32, ptr %1, align 4
  %3 = getelementptr inbounds nuw %Vec, ptr %other1, i32 0, i32 1
  %4 = load i32, ptr %3, align 4
  %5 = icmp eq i32 %2, %4
  br i1 %5, label %log.rhs, label %log.end

log.rhs:                                          ; preds = %entry
  %6 = load ptr, ptr %self.slot, align 8
  %7 = getelementptr inbounds nuw %Vec, ptr %6, i32 0, i32 2
  %8 = load i32, ptr %7, align 4
  %9 = getelementptr inbounds nuw %Vec, ptr %other1, i32 0, i32 2
  %10 = load i32, ptr %9, align 4
  %11 = icmp eq i32 %8, %10
  br label %log.end

log.end:                                          ; preds = %log.rhs, %entry
  %log.phi = phi i1 [ %5, %entry ], [ %11, %log.rhs ]
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err, label %err.ok

err:                                              ; preds = %log.end
  ret i1 false

err.ok:                                           ; preds = %log.end
  ret i1 %log.phi
}

define i1 @Vec___lt__(ptr %self, %Vec %other) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %other1 = alloca %Vec, align 8
  store %Vec %other, ptr %other1, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Vec, ptr %0, i32 0, i32 1
  %2 = load i32, ptr %1, align 4
  %3 = getelementptr inbounds nuw %Vec, ptr %other1, i32 0, i32 1
  %4 = load i32, ptr %3, align 4
  %5 = icmp slt i32 %2, %4
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  ret i1 %5
}

define { ptr, i64 } @Vec___str__(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Vec, ptr %0, i32 0, i32 1
  %2 = load i32, ptr %1, align 4
  %str.len = alloca i64, align 8
  %3 = call ptr @sere_str_i32_data(i32 %2, ptr %str.len)
  %4 = load i64, ptr %str.len, align 4
  %5 = insertvalue { ptr, i64 } undef, ptr %3, 0
  %6 = insertvalue { ptr, i64 } %5, i64 %4, 1
  %cat.len = alloca i64, align 8
  %7 = extractvalue { ptr, i64 } %6, 0
  %8 = extractvalue { ptr, i64 } %6, 1
  %9 = call ptr @sere_str_concat_data(ptr @0, i64 4, ptr %7, i64 %8, ptr %cat.len)
  %10 = load i64, ptr %cat.len, align 4
  %11 = insertvalue { ptr, i64 } undef, ptr %9, 0
  %12 = insertvalue { ptr, i64 } %11, i64 %10, 1
  %cat.len1 = alloca i64, align 8
  %13 = extractvalue { ptr, i64 } %12, 0
  %14 = extractvalue { ptr, i64 } %12, 1
  %15 = call ptr @sere_str_concat_data(ptr %13, i64 %14, ptr @1, i64 2, ptr %cat.len1)
  %16 = load i64, ptr %cat.len1, align 4
  %17 = insertvalue { ptr, i64 } undef, ptr %15, 0
  %18 = insertvalue { ptr, i64 } %17, i64 %16, 1
  %19 = load ptr, ptr %self.slot, align 8
  %20 = getelementptr inbounds nuw %Vec, ptr %19, i32 0, i32 2
  %21 = load i32, ptr %20, align 4
  %str.len2 = alloca i64, align 8
  %22 = call ptr @sere_str_i32_data(i32 %21, ptr %str.len2)
  %23 = load i64, ptr %str.len2, align 4
  %24 = insertvalue { ptr, i64 } undef, ptr %22, 0
  %25 = insertvalue { ptr, i64 } %24, i64 %23, 1
  %cat.len3 = alloca i64, align 8
  %26 = extractvalue { ptr, i64 } %18, 0
  %27 = extractvalue { ptr, i64 } %18, 1
  %28 = extractvalue { ptr, i64 } %25, 0
  %29 = extractvalue { ptr, i64 } %25, 1
  %30 = call ptr @sere_str_concat_data(ptr %26, i64 %27, ptr %28, i64 %29, ptr %cat.len3)
  %31 = load i64, ptr %cat.len3, align 4
  %32 = insertvalue { ptr, i64 } undef, ptr %30, 0
  %33 = insertvalue { ptr, i64 } %32, i64 %31, 1
  %cat.len4 = alloca i64, align 8
  %34 = extractvalue { ptr, i64 } %33, 0
  %35 = extractvalue { ptr, i64 } %33, 1
  %36 = call ptr @sere_str_concat_data(ptr %34, i64 %35, ptr @2, i64 1, ptr %cat.len4)
  %37 = load i64, ptr %cat.len4, align 4
  %38 = insertvalue { ptr, i64 } undef, ptr %36, 0
  %39 = insertvalue { ptr, i64 } %38, i64 %37, 1
  %40 = call i32 @sere_has_error()
  %41 = icmp ne i32 %40, 0
  br i1 %41, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  ret { ptr, i64 } %39
}

define void @Money___init__(ptr %self, i32 %cents) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %cents1 = alloca i32, align 4
  store i32 %cents, ptr %cents1, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Money, ptr %0, i32 0, i32 1
  %2 = load i32, ptr %cents1, align 4
  store i32 %2, ptr %1, align 4
  ret void
}

define %Money @Money___add__(ptr %self, %Money %other) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %other1 = alloca %Money, align 8
  store %Money %other, ptr %other1, align 4
  %init.tmp = alloca %Money, align 8
  store %Money zeroinitializer, ptr %init.tmp, align 4
  %0 = getelementptr inbounds nuw %Money, ptr %init.tmp, i32 0, i32 0
  store i32 -1286910993, ptr %0, align 4
  %1 = load ptr, ptr %self.slot, align 8
  %2 = getelementptr inbounds nuw %Money, ptr %1, i32 0, i32 1
  %3 = load i32, ptr %2, align 4
  %4 = getelementptr inbounds nuw %Money, ptr %other1, i32 0, i32 1
  %5 = load i32, ptr %4, align 4
  %6 = add i32 %3, %5
  call void @Money___init__(ptr %init.tmp, i32 %6)
  %7 = load %Money, ptr %init.tmp, align 4
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Money zeroinitializer

err.ok:                                           ; preds = %entry
  %10 = call i32 @sere_has_error()
  %11 = icmp ne i32 %10, 0
  br i1 %11, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Money zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Money %7
}

define i1 @Money___eq__(ptr %self, %Money %other) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %other1 = alloca %Money, align 8
  store %Money %other, ptr %other1, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Money, ptr %0, i32 0, i32 1
  %2 = load i32, ptr %1, align 4
  %3 = getelementptr inbounds nuw %Money, ptr %other1, i32 0, i32 1
  %4 = load i32, ptr %3, align 4
  %5 = icmp eq i32 %2, %4
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  ret i1 %5
}

define { ptr, i64 } @Money___str__(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Money, ptr %0, i32 0, i32 1
  %2 = load i32, ptr %1, align 4
  %str.len = alloca i64, align 8
  %3 = call ptr @sere_str_i32_data(i32 %2, ptr %str.len)
  %4 = load i64, ptr %str.len, align 4
  %5 = insertvalue { ptr, i64 } undef, ptr %3, 0
  %6 = insertvalue { ptr, i64 } %5, i64 %4, 1
  %cat.len = alloca i64, align 8
  %7 = extractvalue { ptr, i64 } %6, 0
  %8 = extractvalue { ptr, i64 } %6, 1
  %9 = call ptr @sere_str_concat_data(ptr %7, i64 %8, ptr @3, i64 1, ptr %cat.len)
  %10 = load i64, ptr %cat.len, align 4
  %11 = insertvalue { ptr, i64 } undef, ptr %9, 0
  %12 = insertvalue { ptr, i64 } %11, i64 %10, 1
  %13 = call i32 @sere_has_error()
  %14 = icmp ne i32 %13, 0
  br i1 %14, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  ret { ptr, i64 } %12
}

define void @Object___init__(ptr %self, { ptr, i64 } %part) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %part1 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %part, ptr %part1, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Object, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %part1, align 8
  store { ptr, i64 } %2, ptr %1, align 8
  ret void
}

define %Object @Object___add__(ptr %self, %Object %other) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %other1 = alloca %Object, align 8
  store %Object %other, ptr %other1, align 8
  %init.tmp = alloca %Object, align 8
  store %Object zeroinitializer, ptr %init.tmp, align 8
  %0 = getelementptr inbounds nuw %Object, ptr %init.tmp, i32 0, i32 0
  store i32 -443652902, ptr %0, align 4
  %1 = load ptr, ptr %self.slot, align 8
  %2 = getelementptr inbounds nuw %Object, ptr %1, i32 0, i32 1
  %3 = load { ptr, i64 }, ptr %2, align 8
  %4 = getelementptr inbounds nuw %Object, ptr %other1, i32 0, i32 1
  %5 = load { ptr, i64 }, ptr %4, align 8
  %cat.len = alloca i64, align 8
  %6 = extractvalue { ptr, i64 } %3, 0
  %7 = extractvalue { ptr, i64 } %3, 1
  %8 = extractvalue { ptr, i64 } %5, 0
  %9 = extractvalue { ptr, i64 } %5, 1
  %10 = call ptr @sere_str_concat_data(ptr %6, i64 %7, ptr %8, i64 %9, ptr %cat.len)
  %11 = load i64, ptr %cat.len, align 4
  %12 = insertvalue { ptr, i64 } undef, ptr %10, 0
  %13 = insertvalue { ptr, i64 } %12, i64 %11, 1
  call void @Object___init__(ptr %init.tmp, { ptr, i64 } %13)
  %14 = load %Object, ptr %init.tmp, align 8
  %15 = call i32 @sere_has_error()
  %16 = icmp ne i32 %15, 0
  br i1 %16, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Object zeroinitializer

err.ok:                                           ; preds = %entry
  %17 = call i32 @sere_has_error()
  %18 = icmp ne i32 %17, 0
  br i1 %18, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Object zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Object %14
}

define { ptr, i64 } @Object___str__(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Object, ptr %0, i32 0, i32 1
  %2 = load { ptr, i64 }, ptr %1, align 8
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %entry
  ret { ptr, i64 } %2
}

define i32 @sere_main(ptr %argv) {
entry:
  %o2 = alloca %Object, align 8
  %o1 = alloca %Object, align 8
  %m = alloca %Money, align 8
  %c = alloca %Vec, align 8
  %b = alloca %Vec, align 8
  %a = alloca %Vec, align 8
  %zs = alloca ptr, align 8
  %ys = alloca ptr, align 8
  %xs = alloca ptr, align 8
  %s = alloca { ptr, i64 }, align 8
  %argv1 = alloca ptr, align 8
  store ptr %argv, ptr %argv1, align 8
  store { ptr, i64 } { ptr @4, i64 5 }, ptr %s, align 8
  %0 = load { ptr, i64 }, ptr %s, align 8
  %cat.len = alloca i64, align 8
  %1 = extractvalue { ptr, i64 } %0, 0
  %2 = extractvalue { ptr, i64 } %0, 1
  %3 = call ptr @sere_str_concat_data(ptr %1, i64 %2, ptr @5, i64 5, ptr %cat.len)
  %4 = load i64, ptr %cat.len, align 4
  %5 = insertvalue { ptr, i64 } undef, ptr %3, 0
  %6 = insertvalue { ptr, i64 } %5, i64 %4, 1
  store { ptr, i64 } %6, ptr %s, align 8
  %7 = load { ptr, i64 }, ptr %s, align 8
  %8 = extractvalue { ptr, i64 } %7, 0
  %9 = extractvalue { ptr, i64 } %7, 1
  %10 = call i32 @sere_str_cmp(ptr %8, i64 %9, ptr @6, i64 10)
  %11 = icmp eq i32 %10, 0
  br i1 %11, label %assert.ok, label %assert.fail

assert.fail:                                      ; preds = %entry
  call void @sere_raise(ptr @8, ptr @7, i64 16)
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err, label %err.ok

assert.ok:                                        ; preds = %entry
  %14 = load { ptr, i64 }, ptr %s, align 8
  %cat.len2 = alloca i64, align 8
  %15 = extractvalue { ptr, i64 } %14, 0
  %16 = extractvalue { ptr, i64 } %14, 1
  %17 = call ptr @sere_str_concat_data(ptr %15, i64 %16, ptr @9, i64 2, ptr %cat.len2)
  %18 = load i64, ptr %cat.len2, align 4
  %19 = insertvalue { ptr, i64 } undef, ptr %17, 0
  %20 = insertvalue { ptr, i64 } %19, i64 %18, 1
  %str.len = alloca i64, align 8
  %21 = call ptr @sere_str_i32_data(i32 5, ptr %str.len)
  %22 = load i64, ptr %str.len, align 4
  %23 = insertvalue { ptr, i64 } undef, ptr %21, 0
  %24 = insertvalue { ptr, i64 } %23, i64 %22, 1
  %cat.len3 = alloca i64, align 8
  %25 = extractvalue { ptr, i64 } %20, 0
  %26 = extractvalue { ptr, i64 } %20, 1
  %27 = extractvalue { ptr, i64 } %24, 0
  %28 = extractvalue { ptr, i64 } %24, 1
  %29 = call ptr @sere_str_concat_data(ptr %25, i64 %26, ptr %27, i64 %28, ptr %cat.len3)
  %30 = load i64, ptr %cat.len3, align 4
  %31 = insertvalue { ptr, i64 } undef, ptr %29, 0
  %32 = insertvalue { ptr, i64 } %31, i64 %30, 1
  store { ptr, i64 } %32, ptr %s, align 8
  %33 = load { ptr, i64 }, ptr %s, align 8
  %34 = extractvalue { ptr, i64 } %33, 0
  %35 = extractvalue { ptr, i64 } %33, 1
  %36 = call i32 @sere_str_cmp(ptr %34, i64 %35, ptr @10, i64 13)
  %37 = icmp eq i32 %36, 0
  br i1 %37, label %assert.ok5, label %assert.fail4

err:                                              ; preds = %assert.fail
  ret i32 0

err.ok:                                           ; preds = %assert.fail
  unreachable

assert.fail4:                                     ; preds = %assert.ok
  call void @sere_raise(ptr @12, ptr @11, i64 16)
  %38 = call i32 @sere_has_error()
  %39 = icmp ne i32 %38, 0
  br i1 %39, label %err6, label %err.ok7

assert.ok5:                                       ; preds = %assert.ok
  %40 = load { ptr, i64 }, ptr %s, align 8
  %41 = extractvalue { ptr, i64 } %40, 0
  %42 = extractvalue { ptr, i64 } %40, 1
  call void @sere_write(ptr %41, i64 %42)
  call void @sere_write_nl()
  %43 = call i32 @sere_has_error()
  %44 = icmp ne i32 %43, 0
  br i1 %44, label %err8, label %err.ok9

err6:                                             ; preds = %assert.fail4
  ret i32 0

err.ok7:                                          ; preds = %assert.fail4
  unreachable

err8:                                             ; preds = %assert.ok5
  ret i32 0

err.ok9:                                          ; preds = %assert.ok5
  %str.len10 = alloca i64, align 8
  %45 = call ptr @sere_str_i32_data(i32 7, ptr %str.len10)
  %46 = load i64, ptr %str.len10, align 4
  %47 = insertvalue { ptr, i64 } undef, ptr %45, 0
  %48 = insertvalue { ptr, i64 } %47, i64 %46, 1
  %49 = load { ptr, i64 }, ptr %s, align 8
  %cat.len11 = alloca i64, align 8
  %50 = extractvalue { ptr, i64 } %49, 0
  %51 = extractvalue { ptr, i64 } %49, 1
  %52 = extractvalue { ptr, i64 } %48, 0
  %53 = extractvalue { ptr, i64 } %48, 1
  %54 = call ptr @sere_str_concat_data(ptr %50, i64 %51, ptr %52, i64 %53, ptr %cat.len11)
  %55 = load i64, ptr %cat.len11, align 4
  %56 = insertvalue { ptr, i64 } undef, ptr %54, 0
  %57 = insertvalue { ptr, i64 } %56, i64 %55, 1
  store { ptr, i64 } %57, ptr %s, align 8
  %58 = load { ptr, i64 }, ptr %s, align 8
  %59 = extractvalue { ptr, i64 } %58, 0
  %60 = extractvalue { ptr, i64 } %58, 1
  call void @sere_write(ptr %59, i64 %60)
  call void @sere_write_nl()
  %61 = call i32 @sere_has_error()
  %62 = icmp ne i32 %61, 0
  br i1 %62, label %err12, label %err.ok13

err12:                                            ; preds = %err.ok9
  ret i32 0

err.ok13:                                         ; preds = %err.ok9
  %str.len14 = alloca i64, align 8
  %63 = call ptr @sere_str_f64_data(double 1.500000e+00, ptr %str.len14)
  %64 = load i64, ptr %str.len14, align 4
  %65 = insertvalue { ptr, i64 } undef, ptr %63, 0
  %66 = insertvalue { ptr, i64 } %65, i64 %64, 1
  %cat.len15 = alloca i64, align 8
  %67 = extractvalue { ptr, i64 } %66, 0
  %68 = extractvalue { ptr, i64 } %66, 1
  %69 = call ptr @sere_str_concat_data(ptr %67, i64 %68, ptr @15, i64 2, ptr %cat.len15)
  %70 = load i64, ptr %cat.len15, align 4
  %71 = insertvalue { ptr, i64 } undef, ptr %69, 0
  %72 = insertvalue { ptr, i64 } %71, i64 %70, 1
  %str.len16 = alloca i64, align 8
  %73 = call ptr @sere_str_bool_data(i8 1, ptr %str.len16)
  %74 = load i64, ptr %str.len16, align 4
  %75 = insertvalue { ptr, i64 } undef, ptr %73, 0
  %76 = insertvalue { ptr, i64 } %75, i64 %74, 1
  %cat.len17 = alloca i64, align 8
  %77 = extractvalue { ptr, i64 } %72, 0
  %78 = extractvalue { ptr, i64 } %72, 1
  %79 = extractvalue { ptr, i64 } %76, 0
  %80 = extractvalue { ptr, i64 } %76, 1
  %81 = call ptr @sere_str_concat_data(ptr %77, i64 %78, ptr %79, i64 %80, ptr %cat.len17)
  %82 = load i64, ptr %cat.len17, align 4
  %83 = insertvalue { ptr, i64 } undef, ptr %81, 0
  %84 = insertvalue { ptr, i64 } %83, i64 %82, 1
  %cat.len18 = alloca i64, align 8
  %85 = extractvalue { ptr, i64 } %84, 0
  %86 = extractvalue { ptr, i64 } %84, 1
  %87 = call ptr @sere_str_concat_data(ptr %85, i64 %86, ptr @16, i64 2, ptr %cat.len18)
  %88 = load i64, ptr %cat.len18, align 4
  %89 = insertvalue { ptr, i64 } undef, ptr %87, 0
  %90 = insertvalue { ptr, i64 } %89, i64 %88, 1
  %cat.len19 = alloca i64, align 8
  %91 = extractvalue { ptr, i64 } %90, 0
  %92 = extractvalue { ptr, i64 } %90, 1
  %93 = call ptr @sere_str_concat_data(ptr %91, i64 %92, ptr @17, i64 3, ptr %cat.len19)
  %94 = load i64, ptr %cat.len19, align 4
  %95 = insertvalue { ptr, i64 } undef, ptr %93, 0
  %96 = insertvalue { ptr, i64 } %95, i64 %94, 1
  store { ptr, i64 } %96, ptr %s, align 8
  %97 = load { ptr, i64 }, ptr %s, align 8
  %98 = extractvalue { ptr, i64 } %97, 0
  %99 = extractvalue { ptr, i64 } %97, 1
  call void @sere_write(ptr %98, i64 %99)
  call void @sere_write_nl()
  %100 = call i32 @sere_has_error()
  %101 = icmp ne i32 %100, 0
  br i1 %101, label %err20, label %err.ok21

err20:                                            ; preds = %err.ok13
  ret i32 0

err.ok21:                                         ; preds = %err.ok13
  %rep.data = alloca ptr, align 8
  %rep.len = alloca i64, align 8
  call void @sere_str_repeat(ptr @19, i64 5, i64 5, ptr %rep.data, ptr %rep.len)
  %102 = load i64, ptr %rep.len, align 4
  %103 = load ptr, ptr %rep.data, align 8
  %104 = insertvalue { ptr, i64 } undef, ptr %103, 0
  %105 = insertvalue { ptr, i64 } %104, i64 %102, 1
  %106 = extractvalue { ptr, i64 } %105, 0
  %107 = extractvalue { ptr, i64 } %105, 1
  %108 = call i32 @sere_str_cmp(ptr %106, i64 %107, ptr @20, i64 25)
  %109 = icmp eq i32 %108, 0
  br i1 %109, label %assert.ok23, label %assert.fail22

assert.fail22:                                    ; preds = %err.ok21
  call void @sere_raise(ptr @22, ptr @21, i64 16)
  %110 = call i32 @sere_has_error()
  %111 = icmp ne i32 %110, 0
  br i1 %111, label %err24, label %err.ok25

assert.ok23:                                      ; preds = %err.ok21
  %rep.data26 = alloca ptr, align 8
  %rep.len27 = alloca i64, align 8
  call void @sere_str_repeat(ptr @23, i64 2, i64 3, ptr %rep.data26, ptr %rep.len27)
  %112 = load i64, ptr %rep.len27, align 4
  %113 = load ptr, ptr %rep.data26, align 8
  %114 = insertvalue { ptr, i64 } undef, ptr %113, 0
  %115 = insertvalue { ptr, i64 } %114, i64 %112, 1
  %116 = extractvalue { ptr, i64 } %115, 0
  %117 = extractvalue { ptr, i64 } %115, 1
  %118 = call i32 @sere_str_cmp(ptr %116, i64 %117, ptr @24, i64 6)
  %119 = icmp eq i32 %118, 0
  br i1 %119, label %assert.ok29, label %assert.fail28

err24:                                            ; preds = %assert.fail22
  ret i32 0

err.ok25:                                         ; preds = %assert.fail22
  unreachable

assert.fail28:                                    ; preds = %assert.ok23
  call void @sere_raise(ptr @26, ptr @25, i64 16)
  %120 = call i32 @sere_has_error()
  %121 = icmp ne i32 %120, 0
  br i1 %121, label %err30, label %err.ok31

assert.ok29:                                      ; preds = %assert.ok23
  %rep.data32 = alloca ptr, align 8
  %rep.len33 = alloca i64, align 8
  call void @sere_str_repeat(ptr @28, i64 2, i64 3, ptr %rep.data32, ptr %rep.len33)
  %122 = load i64, ptr %rep.len33, align 4
  %123 = load ptr, ptr %rep.data32, align 8
  %124 = insertvalue { ptr, i64 } undef, ptr %123, 0
  %125 = insertvalue { ptr, i64 } %124, i64 %122, 1
  %126 = extractvalue { ptr, i64 } %125, 0
  %127 = extractvalue { ptr, i64 } %125, 1
  call void @sere_write(ptr %126, i64 %127)
  call void @sere_write_nl()
  %128 = call i32 @sere_has_error()
  %129 = icmp ne i32 %128, 0
  br i1 %129, label %err34, label %err.ok35

err30:                                            ; preds = %assert.fail28
  ret i32 0

err.ok31:                                         ; preds = %assert.fail28
  unreachable

err34:                                            ; preds = %assert.ok29
  ret i32 0

err.ok35:                                         ; preds = %assert.ok29
  %130 = call ptr @sere_list_new(i64 4)
  %tmp.slot = alloca i32, align 4
  store i32 0, ptr %tmp.slot, align 4
  call void @sere_list_push(ptr %130, ptr %tmp.slot)
  %131 = call ptr @sere_list_repeat(ptr %130, i64 10)
  store ptr %131, ptr %xs, align 8
  %132 = load ptr, ptr %xs, align 8
  %133 = call i64 @sere_list_len(ptr %132)
  %134 = call i32 @sere_has_error()
  %135 = icmp ne i32 %134, 0
  br i1 %135, label %err36, label %err.ok37

err36:                                            ; preds = %err.ok35
  ret i32 0

err.ok37:                                         ; preds = %err.ok35
  %136 = icmp eq i64 %133, 10
  br i1 %136, label %assert.ok39, label %assert.fail38

assert.fail38:                                    ; preds = %err.ok37
  call void @sere_raise(ptr @30, ptr @29, i64 16)
  %137 = call i32 @sere_has_error()
  %138 = icmp ne i32 %137, 0
  br i1 %138, label %err40, label %err.ok41

assert.ok39:                                      ; preds = %err.ok37
  %139 = load ptr, ptr %xs, align 8
  %140 = call ptr @sere_list_item(ptr %139, i64 0)
  %141 = load i32, ptr %140, align 4
  %142 = icmp eq i32 %141, 0
  br i1 %142, label %log.rhs, label %log.end

err40:                                            ; preds = %assert.fail38
  ret i32 0

err.ok41:                                         ; preds = %assert.fail38
  unreachable

log.rhs:                                          ; preds = %assert.ok39
  %143 = load ptr, ptr %xs, align 8
  %144 = call ptr @sere_list_item(ptr %143, i64 9)
  %145 = load i32, ptr %144, align 4
  %146 = icmp eq i32 %145, 0
  br label %log.end

log.end:                                          ; preds = %log.rhs, %assert.ok39
  %log.phi = phi i1 [ %142, %assert.ok39 ], [ %146, %log.rhs ]
  br i1 %log.phi, label %assert.ok43, label %assert.fail42

assert.fail42:                                    ; preds = %log.end
  call void @sere_raise(ptr @32, ptr @31, i64 16)
  %147 = call i32 @sere_has_error()
  %148 = icmp ne i32 %147, 0
  br i1 %148, label %err44, label %err.ok45

assert.ok43:                                      ; preds = %log.end
  %149 = load ptr, ptr %xs, align 8
  %150 = call { ptr, i64 } @"__sere_repr_list[i32]"(ptr %149)
  %151 = extractvalue { ptr, i64 } %150, 0
  %152 = extractvalue { ptr, i64 } %150, 1
  call void @sere_write(ptr %151, i64 %152)
  call void @sere_write_nl()
  %153 = call i32 @sere_has_error()
  %154 = icmp ne i32 %153, 0
  br i1 %154, label %err46, label %err.ok47

err44:                                            ; preds = %assert.fail42
  ret i32 0

err.ok45:                                         ; preds = %assert.fail42
  unreachable

err46:                                            ; preds = %assert.ok43
  ret i32 0

err.ok47:                                         ; preds = %assert.ok43
  %155 = call ptr @sere_list_new(i64 4)
  %tmp.slot48 = alloca i32, align 4
  store i32 1, ptr %tmp.slot48, align 4
  call void @sere_list_push(ptr %155, ptr %tmp.slot48)
  %tmp.slot49 = alloca i32, align 4
  store i32 2, ptr %tmp.slot49, align 4
  call void @sere_list_push(ptr %155, ptr %tmp.slot49)
  %156 = call ptr @sere_list_repeat(ptr %155, i64 3)
  store ptr %156, ptr %ys, align 8
  %157 = load ptr, ptr %ys, align 8
  %158 = call i64 @sere_list_len(ptr %157)
  %159 = call i32 @sere_has_error()
  %160 = icmp ne i32 %159, 0
  br i1 %160, label %err50, label %err.ok51

err50:                                            ; preds = %err.ok47
  ret i32 0

err.ok51:                                         ; preds = %err.ok47
  %161 = icmp eq i64 %158, 6
  br i1 %161, label %assert.ok53, label %assert.fail52

assert.fail52:                                    ; preds = %err.ok51
  call void @sere_raise(ptr @39, ptr @38, i64 16)
  %162 = call i32 @sere_has_error()
  %163 = icmp ne i32 %162, 0
  br i1 %163, label %err54, label %err.ok55

assert.ok53:                                      ; preds = %err.ok51
  %164 = load ptr, ptr %ys, align 8
  %165 = call { ptr, i64 } @"__sere_repr_list[i32]"(ptr %164)
  %166 = extractvalue { ptr, i64 } %165, 0
  %167 = extractvalue { ptr, i64 } %165, 1
  call void @sere_write(ptr %166, i64 %167)
  call void @sere_write_nl()
  %168 = call i32 @sere_has_error()
  %169 = icmp ne i32 %168, 0
  br i1 %169, label %err56, label %err.ok57

err54:                                            ; preds = %assert.fail52
  ret i32 0

err.ok55:                                         ; preds = %assert.fail52
  unreachable

err56:                                            ; preds = %assert.ok53
  ret i32 0

err.ok57:                                         ; preds = %assert.ok53
  %170 = call ptr @sere_list_new(i64 4)
  %tmp.slot58 = alloca i32, align 4
  store i32 7, ptr %tmp.slot58, align 4
  call void @sere_list_push(ptr %170, ptr %tmp.slot58)
  %171 = call ptr @sere_list_repeat(ptr %170, i64 2)
  store ptr %171, ptr %zs, align 8
  %172 = load ptr, ptr %zs, align 8
  %173 = call { ptr, i64 } @"__sere_repr_list[i32]"(ptr %172)
  %174 = extractvalue { ptr, i64 } %173, 0
  %175 = extractvalue { ptr, i64 } %173, 1
  call void @sere_write(ptr %174, i64 %175)
  call void @sere_write_nl()
  %176 = call i32 @sere_has_error()
  %177 = icmp ne i32 %176, 0
  br i1 %177, label %err59, label %err.ok60

err59:                                            ; preds = %err.ok57
  ret i32 0

err.ok60:                                         ; preds = %err.ok57
  %178 = call ptr @sere_list_new(i64 4)
  %tmp.slot61 = alloca i32, align 4
  store i32 9, ptr %tmp.slot61, align 4
  call void @sere_list_push(ptr %178, ptr %tmp.slot61)
  %179 = load ptr, ptr %ys, align 8
  %180 = call ptr @sere_list_concat(ptr %179, ptr %178)
  store ptr %180, ptr %ys, align 8
  %181 = load ptr, ptr %ys, align 8
  %182 = call i64 @sere_list_len(ptr %181)
  %183 = call i32 @sere_has_error()
  %184 = icmp ne i32 %183, 0
  br i1 %184, label %err62, label %err.ok63

err62:                                            ; preds = %err.ok60
  ret i32 0

err.ok63:                                         ; preds = %err.ok60
  %185 = icmp eq i64 %182, 7
  br i1 %185, label %assert.ok65, label %assert.fail64

assert.fail64:                                    ; preds = %err.ok63
  call void @sere_raise(ptr @43, ptr @42, i64 16)
  %186 = call i32 @sere_has_error()
  %187 = icmp ne i32 %186, 0
  br i1 %187, label %err66, label %err.ok67

assert.ok65:                                      ; preds = %err.ok63
  %188 = load ptr, ptr %ys, align 8
  %189 = call { ptr, i64 } @"__sere_repr_list[i32]"(ptr %188)
  %190 = extractvalue { ptr, i64 } %189, 0
  %191 = extractvalue { ptr, i64 } %189, 1
  call void @sere_write(ptr %190, i64 %191)
  call void @sere_write_nl()
  %192 = call i32 @sere_has_error()
  %193 = icmp ne i32 %192, 0
  br i1 %193, label %err68, label %err.ok69

err66:                                            ; preds = %assert.fail64
  ret i32 0

err.ok67:                                         ; preds = %assert.fail64
  unreachable

err68:                                            ; preds = %assert.ok65
  ret i32 0

err.ok69:                                         ; preds = %assert.ok65
  %init.tmp = alloca %Vec, align 8
  store %Vec zeroinitializer, ptr %init.tmp, align 4
  %194 = getelementptr inbounds nuw %Vec, ptr %init.tmp, i32 0, i32 0
  store i32 -709171067, ptr %194, align 4
  call void @Vec___init__(ptr %init.tmp, i32 1, i32 2)
  %195 = load %Vec, ptr %init.tmp, align 4
  %196 = call i32 @sere_has_error()
  %197 = icmp ne i32 %196, 0
  br i1 %197, label %err70, label %err.ok71

err70:                                            ; preds = %err.ok69
  ret i32 0

err.ok71:                                         ; preds = %err.ok69
  store %Vec %195, ptr %a, align 4
  %init.tmp72 = alloca %Vec, align 8
  store %Vec zeroinitializer, ptr %init.tmp72, align 4
  %198 = getelementptr inbounds nuw %Vec, ptr %init.tmp72, i32 0, i32 0
  store i32 -709171067, ptr %198, align 4
  call void @Vec___init__(ptr %init.tmp72, i32 3, i32 4)
  %199 = load %Vec, ptr %init.tmp72, align 4
  %200 = call i32 @sere_has_error()
  %201 = icmp ne i32 %200, 0
  br i1 %201, label %err73, label %err.ok74

err73:                                            ; preds = %err.ok71
  ret i32 0

err.ok74:                                         ; preds = %err.ok71
  store %Vec %199, ptr %b, align 4
  %202 = load %Vec, ptr %b, align 4
  %203 = call %Vec @Vec___add__(ptr %a, %Vec %202)
  %204 = call i32 @sere_has_error()
  %205 = icmp ne i32 %204, 0
  br i1 %205, label %err75, label %err.ok76

err75:                                            ; preds = %err.ok74
  ret i32 0

err.ok76:                                         ; preds = %err.ok74
  %str.tmp = alloca %Vec, align 8
  store %Vec %203, ptr %str.tmp, align 4
  %206 = call { ptr, i64 } @Vec___str__(ptr %str.tmp)
  %207 = extractvalue { ptr, i64 } %206, 0
  %208 = extractvalue { ptr, i64 } %206, 1
  call void @sere_write(ptr %207, i64 %208)
  call void @sere_write_nl()
  %209 = call i32 @sere_has_error()
  %210 = icmp ne i32 %209, 0
  br i1 %210, label %err77, label %err.ok78

err77:                                            ; preds = %err.ok76
  ret i32 0

err.ok78:                                         ; preds = %err.ok76
  %211 = load %Vec, ptr %a, align 4
  %212 = call %Vec @Vec___sub__(ptr %b, %Vec %211)
  %213 = call i32 @sere_has_error()
  %214 = icmp ne i32 %213, 0
  br i1 %214, label %err79, label %err.ok80

err79:                                            ; preds = %err.ok78
  ret i32 0

err.ok80:                                         ; preds = %err.ok78
  %str.tmp81 = alloca %Vec, align 8
  store %Vec %212, ptr %str.tmp81, align 4
  %215 = call { ptr, i64 } @Vec___str__(ptr %str.tmp81)
  %216 = extractvalue { ptr, i64 } %215, 0
  %217 = extractvalue { ptr, i64 } %215, 1
  call void @sere_write(ptr %216, i64 %217)
  call void @sere_write_nl()
  %218 = call i32 @sere_has_error()
  %219 = icmp ne i32 %218, 0
  br i1 %219, label %err82, label %err.ok83

err82:                                            ; preds = %err.ok80
  ret i32 0

err.ok83:                                         ; preds = %err.ok80
  %220 = call %Vec @Vec___mul__(ptr %a, i32 3)
  %221 = call i32 @sere_has_error()
  %222 = icmp ne i32 %221, 0
  br i1 %222, label %err84, label %err.ok85

err84:                                            ; preds = %err.ok83
  ret i32 0

err.ok85:                                         ; preds = %err.ok83
  %str.tmp86 = alloca %Vec, align 8
  store %Vec %220, ptr %str.tmp86, align 4
  %223 = call { ptr, i64 } @Vec___str__(ptr %str.tmp86)
  %224 = extractvalue { ptr, i64 } %223, 0
  %225 = extractvalue { ptr, i64 } %223, 1
  call void @sere_write(ptr %224, i64 %225)
  call void @sere_write_nl()
  %226 = call i32 @sere_has_error()
  %227 = icmp ne i32 %226, 0
  br i1 %227, label %err87, label %err.ok88

err87:                                            ; preds = %err.ok85
  ret i32 0

err.ok88:                                         ; preds = %err.ok85
  %init.tmp89 = alloca %Vec, align 8
  store %Vec zeroinitializer, ptr %init.tmp89, align 4
  %228 = getelementptr inbounds nuw %Vec, ptr %init.tmp89, i32 0, i32 0
  store i32 -709171067, ptr %228, align 4
  call void @Vec___init__(ptr %init.tmp89, i32 1, i32 2)
  %229 = load %Vec, ptr %init.tmp89, align 4
  %230 = call i32 @sere_has_error()
  %231 = icmp ne i32 %230, 0
  br i1 %231, label %err90, label %err.ok91

err90:                                            ; preds = %err.ok88
  ret i32 0

err.ok91:                                         ; preds = %err.ok88
  %232 = call i1 @Vec___eq__(ptr %a, %Vec %229)
  %233 = call i32 @sere_has_error()
  %234 = icmp ne i32 %233, 0
  br i1 %234, label %err92, label %err.ok93

err92:                                            ; preds = %err.ok91
  ret i32 0

err.ok93:                                         ; preds = %err.ok91
  br i1 %232, label %assert.ok95, label %assert.fail94

assert.fail94:                                    ; preds = %err.ok93
  call void @sere_raise(ptr @49, ptr @48, i64 16)
  %235 = call i32 @sere_has_error()
  %236 = icmp ne i32 %235, 0
  br i1 %236, label %err96, label %err.ok97

assert.ok95:                                      ; preds = %err.ok93
  %237 = load %Vec, ptr %b, align 4
  %238 = call i1 @Vec___eq__(ptr %a, %Vec %237)
  %239 = call i32 @sere_has_error()
  %240 = icmp ne i32 %239, 0
  br i1 %240, label %err98, label %err.ok99

err96:                                            ; preds = %assert.fail94
  ret i32 0

err.ok97:                                         ; preds = %assert.fail94
  unreachable

err98:                                            ; preds = %assert.ok95
  ret i32 0

err.ok99:                                         ; preds = %assert.ok95
  %241 = xor i1 %238, true
  br i1 %241, label %assert.ok101, label %assert.fail100

assert.fail100:                                   ; preds = %err.ok99
  call void @sere_raise(ptr @51, ptr @50, i64 16)
  %242 = call i32 @sere_has_error()
  %243 = icmp ne i32 %242, 0
  br i1 %243, label %err102, label %err.ok103

assert.ok101:                                     ; preds = %err.ok99
  %244 = load %Vec, ptr %b, align 4
  %245 = call i1 @Vec___lt__(ptr %a, %Vec %244)
  %246 = call i32 @sere_has_error()
  %247 = icmp ne i32 %246, 0
  br i1 %247, label %err104, label %err.ok105

err102:                                           ; preds = %assert.fail100
  ret i32 0

err.ok103:                                        ; preds = %assert.fail100
  unreachable

err104:                                           ; preds = %assert.ok101
  ret i32 0

err.ok105:                                        ; preds = %assert.ok101
  br i1 %245, label %assert.ok107, label %assert.fail106

assert.fail106:                                   ; preds = %err.ok105
  call void @sere_raise(ptr @53, ptr @52, i64 16)
  %248 = call i32 @sere_has_error()
  %249 = icmp ne i32 %248, 0
  br i1 %249, label %err108, label %err.ok109

assert.ok107:                                     ; preds = %err.ok105
  %250 = load %Vec, ptr %b, align 4
  %251 = call i1 @Vec___lt__(ptr %a, %Vec %250)
  %252 = call i32 @sere_has_error()
  %253 = icmp ne i32 %252, 0
  br i1 %253, label %err110, label %err.ok111

err108:                                           ; preds = %assert.fail106
  ret i32 0

err.ok109:                                        ; preds = %assert.fail106
  unreachable

err110:                                           ; preds = %assert.ok107
  ret i32 0

err.ok111:                                        ; preds = %assert.ok107
  br i1 %251, label %assert.ok113, label %assert.fail112

assert.fail112:                                   ; preds = %err.ok111
  call void @sere_raise(ptr @55, ptr @54, i64 16)
  %254 = call i32 @sere_has_error()
  %255 = icmp ne i32 %254, 0
  br i1 %255, label %err114, label %err.ok115

assert.ok113:                                     ; preds = %err.ok111
  %256 = load %Vec, ptr %a, align 4
  %init.tmp116 = alloca %Vec, align 8
  store %Vec zeroinitializer, ptr %init.tmp116, align 4
  %257 = getelementptr inbounds nuw %Vec, ptr %init.tmp116, i32 0, i32 0
  store i32 -709171067, ptr %257, align 4
  call void @Vec___init__(ptr %init.tmp116, i32 1, i32 2)
  %258 = load %Vec, ptr %init.tmp116, align 4
  %259 = call i32 @sere_has_error()
  %260 = icmp ne i32 %259, 0
  br i1 %260, label %err117, label %err.ok118

err114:                                           ; preds = %assert.fail112
  ret i32 0

err.ok115:                                        ; preds = %assert.fail112
  unreachable

err117:                                           ; preds = %assert.ok113
  ret i32 0

err.ok118:                                        ; preds = %assert.ok113
  br i1 false, label %assert.ok120, label %assert.fail119

assert.fail119:                                   ; preds = %err.ok118
  call void @sere_raise(ptr @57, ptr @56, i64 16)
  %261 = call i32 @sere_has_error()
  %262 = icmp ne i32 %261, 0
  br i1 %262, label %err121, label %err.ok122

assert.ok120:                                     ; preds = %err.ok118
  %263 = load %Vec, ptr %b, align 4
  %264 = load %Vec, ptr %a, align 4
  br i1 false, label %assert.ok124, label %assert.fail123

err121:                                           ; preds = %assert.fail119
  ret i32 0

err.ok122:                                        ; preds = %assert.fail119
  unreachable

assert.fail123:                                   ; preds = %assert.ok120
  call void @sere_raise(ptr @59, ptr @58, i64 16)
  %265 = call i32 @sere_has_error()
  %266 = icmp ne i32 %265, 0
  br i1 %266, label %err125, label %err.ok126

assert.ok124:                                     ; preds = %assert.ok120
  %init.tmp127 = alloca %Vec, align 8
  store %Vec zeroinitializer, ptr %init.tmp127, align 4
  %267 = getelementptr inbounds nuw %Vec, ptr %init.tmp127, i32 0, i32 0
  store i32 -709171067, ptr %267, align 4
  call void @Vec___init__(ptr %init.tmp127, i32 0, i32 0)
  %268 = load %Vec, ptr %init.tmp127, align 4
  %269 = call i32 @sere_has_error()
  %270 = icmp ne i32 %269, 0
  br i1 %270, label %err128, label %err.ok129

err125:                                           ; preds = %assert.fail123
  ret i32 0

err.ok126:                                        ; preds = %assert.fail123
  unreachable

err128:                                           ; preds = %assert.ok124
  ret i32 0

err.ok129:                                        ; preds = %assert.ok124
  store %Vec %268, ptr %c, align 4
  %271 = load %Vec, ptr %a, align 4
  %272 = load %Vec, ptr %c, align 4
  %273 = call %Vec @Vec___add__(ptr %c, %Vec %271)
  %274 = call i32 @sere_has_error()
  %275 = icmp ne i32 %274, 0
  br i1 %275, label %err130, label %err.ok131

err130:                                           ; preds = %err.ok129
  ret i32 0

err.ok131:                                        ; preds = %err.ok129
  store %Vec %273, ptr %c, align 4
  %276 = call { ptr, i64 } @Vec___str__(ptr %c)
  %277 = extractvalue { ptr, i64 } %276, 0
  %278 = extractvalue { ptr, i64 } %276, 1
  call void @sere_write(ptr %277, i64 %278)
  call void @sere_write_nl()
  %279 = call i32 @sere_has_error()
  %280 = icmp ne i32 %279, 0
  br i1 %280, label %err132, label %err.ok133

err132:                                           ; preds = %err.ok131
  ret i32 0

err.ok133:                                        ; preds = %err.ok131
  %281 = load %Vec, ptr %a, align 4
  %282 = call i1 @Vec___eq__(ptr %c, %Vec %281)
  %283 = call i32 @sere_has_error()
  %284 = icmp ne i32 %283, 0
  br i1 %284, label %err134, label %err.ok135

err134:                                           ; preds = %err.ok133
  ret i32 0

err.ok135:                                        ; preds = %err.ok133
  br i1 %282, label %assert.ok137, label %assert.fail136

assert.fail136:                                   ; preds = %err.ok135
  call void @sere_raise(ptr @62, ptr @61, i64 16)
  %285 = call i32 @sere_has_error()
  %286 = icmp ne i32 %285, 0
  br i1 %286, label %err138, label %err.ok139

assert.ok137:                                     ; preds = %err.ok135
  %init.tmp140 = alloca %Money, align 8
  store %Money zeroinitializer, ptr %init.tmp140, align 4
  %287 = getelementptr inbounds nuw %Money, ptr %init.tmp140, i32 0, i32 0
  store i32 -1286910993, ptr %287, align 4
  call void @Money___init__(ptr %init.tmp140, i32 150)
  %288 = load %Money, ptr %init.tmp140, align 4
  %289 = call i32 @sere_has_error()
  %290 = icmp ne i32 %289, 0
  br i1 %290, label %err141, label %err.ok142

err138:                                           ; preds = %assert.fail136
  ret i32 0

err.ok139:                                        ; preds = %assert.fail136
  unreachable

err141:                                           ; preds = %assert.ok137
  ret i32 0

err.ok142:                                        ; preds = %assert.ok137
  store %Money %288, ptr %m, align 4
  %init.tmp143 = alloca %Money, align 8
  store %Money zeroinitializer, ptr %init.tmp143, align 4
  %291 = getelementptr inbounds nuw %Money, ptr %init.tmp143, i32 0, i32 0
  store i32 -1286910993, ptr %291, align 4
  call void @Money___init__(ptr %init.tmp143, i32 75)
  %292 = load %Money, ptr %init.tmp143, align 4
  %293 = call i32 @sere_has_error()
  %294 = icmp ne i32 %293, 0
  br i1 %294, label %err144, label %err.ok145

err144:                                           ; preds = %err.ok142
  ret i32 0

err.ok145:                                        ; preds = %err.ok142
  %295 = load %Money, ptr %m, align 4
  %296 = call %Money @Money___add__(ptr %m, %Money %292)
  %297 = call i32 @sere_has_error()
  %298 = icmp ne i32 %297, 0
  br i1 %298, label %err146, label %err.ok147

err146:                                           ; preds = %err.ok145
  ret i32 0

err.ok147:                                        ; preds = %err.ok145
  store %Money %296, ptr %m, align 4
  %299 = call { ptr, i64 } @Money___str__(ptr %m)
  %300 = extractvalue { ptr, i64 } %299, 0
  %301 = extractvalue { ptr, i64 } %299, 1
  call void @sere_write(ptr %300, i64 %301)
  call void @sere_write_nl()
  %302 = call i32 @sere_has_error()
  %303 = icmp ne i32 %302, 0
  br i1 %303, label %err148, label %err.ok149

err148:                                           ; preds = %err.ok147
  ret i32 0

err.ok149:                                        ; preds = %err.ok147
  %init.tmp150 = alloca %Money, align 8
  store %Money zeroinitializer, ptr %init.tmp150, align 4
  %304 = getelementptr inbounds nuw %Money, ptr %init.tmp150, i32 0, i32 0
  store i32 -1286910993, ptr %304, align 4
  call void @Money___init__(ptr %init.tmp150, i32 225)
  %305 = load %Money, ptr %init.tmp150, align 4
  %306 = call i32 @sere_has_error()
  %307 = icmp ne i32 %306, 0
  br i1 %307, label %err151, label %err.ok152

err151:                                           ; preds = %err.ok149
  ret i32 0

err.ok152:                                        ; preds = %err.ok149
  %308 = call i1 @Money___eq__(ptr %m, %Money %305)
  %309 = call i32 @sere_has_error()
  %310 = icmp ne i32 %309, 0
  br i1 %310, label %err153, label %err.ok154

err153:                                           ; preds = %err.ok152
  ret i32 0

err.ok154:                                        ; preds = %err.ok152
  br i1 %308, label %assert.ok156, label %assert.fail155

assert.fail155:                                   ; preds = %err.ok154
  call void @sere_raise(ptr @65, ptr @64, i64 16)
  %311 = call i32 @sere_has_error()
  %312 = icmp ne i32 %311, 0
  br i1 %312, label %err157, label %err.ok158

assert.ok156:                                     ; preds = %err.ok154
  %init.tmp159 = alloca %Object, align 8
  store %Object zeroinitializer, ptr %init.tmp159, align 8
  %313 = getelementptr inbounds nuw %Object, ptr %init.tmp159, i32 0, i32 0
  store i32 -443652902, ptr %313, align 4
  call void @Object___init__(ptr %init.tmp159, { ptr, i64 } { ptr @66, i64 6 })
  %314 = load %Object, ptr %init.tmp159, align 8
  %315 = call i32 @sere_has_error()
  %316 = icmp ne i32 %315, 0
  br i1 %316, label %err160, label %err.ok161

err157:                                           ; preds = %assert.fail155
  ret i32 0

err.ok158:                                        ; preds = %assert.fail155
  unreachable

err160:                                           ; preds = %assert.ok156
  ret i32 0

err.ok161:                                        ; preds = %assert.ok156
  store %Object %314, ptr %o1, align 8
  %init.tmp162 = alloca %Object, align 8
  store %Object zeroinitializer, ptr %init.tmp162, align 8
  %317 = getelementptr inbounds nuw %Object, ptr %init.tmp162, i32 0, i32 0
  store i32 -443652902, ptr %317, align 4
  call void @Object___init__(ptr %init.tmp162, { ptr, i64 } { ptr @67, i64 5 })
  %318 = load %Object, ptr %init.tmp162, align 8
  %319 = call i32 @sere_has_error()
  %320 = icmp ne i32 %319, 0
  br i1 %320, label %err163, label %err.ok164

err163:                                           ; preds = %err.ok161
  ret i32 0

err.ok164:                                        ; preds = %err.ok161
  store %Object %318, ptr %o2, align 8
  %321 = load %Object, ptr %o2, align 8
  %322 = call %Object @Object___add__(ptr %o1, %Object %321)
  %323 = call i32 @sere_has_error()
  %324 = icmp ne i32 %323, 0
  br i1 %324, label %err165, label %err.ok166

err165:                                           ; preds = %err.ok164
  ret i32 0

err.ok166:                                        ; preds = %err.ok164
  %str.tmp167 = alloca %Object, align 8
  store %Object %322, ptr %str.tmp167, align 8
  %325 = call { ptr, i64 } @Object___str__(ptr %str.tmp167)
  %326 = extractvalue { ptr, i64 } %325, 0
  %327 = extractvalue { ptr, i64 } %325, 1
  call void @sere_write(ptr %326, i64 %327)
  call void @sere_write_nl()
  %328 = call i32 @sere_has_error()
  %329 = icmp ne i32 %328, 0
  br i1 %329, label %err168, label %err.ok169

err168:                                           ; preds = %err.ok166
  ret i32 0

err.ok169:                                        ; preds = %err.ok166
  %330 = load %Object, ptr %o2, align 8
  %331 = call %Object @Object___add__(ptr %o1, %Object %330)
  %332 = call i32 @sere_has_error()
  %333 = icmp ne i32 %332, 0
  br i1 %333, label %err170, label %err.ok171

err170:                                           ; preds = %err.ok169
  ret i32 0

err.ok171:                                        ; preds = %err.ok169
  %init.tmp172 = alloca %Object, align 8
  store %Object zeroinitializer, ptr %init.tmp172, align 8
  %334 = getelementptr inbounds nuw %Object, ptr %init.tmp172, i32 0, i32 0
  store i32 -443652902, ptr %334, align 4
  call void @Object___init__(ptr %init.tmp172, { ptr, i64 } { ptr @69, i64 11 })
  %335 = load %Object, ptr %init.tmp172, align 8
  %336 = call i32 @sere_has_error()
  %337 = icmp ne i32 %336, 0
  br i1 %337, label %err173, label %err.ok174

err173:                                           ; preds = %err.ok171
  ret i32 0

err.ok174:                                        ; preds = %err.ok171
  br i1 false, label %assert.ok176, label %assert.fail175

assert.fail175:                                   ; preds = %err.ok174
  call void @sere_raise(ptr @71, ptr @70, i64 16)
  %338 = call i32 @sere_has_error()
  %339 = icmp ne i32 %338, 0
  br i1 %339, label %err177, label %err.ok178

assert.ok176:                                     ; preds = %err.ok174
  %340 = load %Object, ptr %o1, align 8
  %341 = load %Object, ptr %o2, align 8
  br i1 false, label %assert.ok180, label %assert.fail179

err177:                                           ; preds = %assert.fail175
  ret i32 0

err.ok178:                                        ; preds = %assert.fail175
  unreachable

assert.fail179:                                   ; preds = %assert.ok176
  call void @sere_raise(ptr @73, ptr @72, i64 16)
  %342 = call i32 @sere_has_error()
  %343 = icmp ne i32 %342, 0
  br i1 %343, label %err181, label %err.ok182

assert.ok180:                                     ; preds = %assert.ok176
  %344 = call i32 @sere_has_error()
  %345 = icmp ne i32 %344, 0
  br i1 %345, label %err183, label %err.ok184

err181:                                           ; preds = %assert.fail179
  ret i32 0

err.ok182:                                        ; preds = %assert.fail179
  unreachable

err183:                                           ; preds = %assert.ok180
  ret i32 0

err.ok184:                                        ; preds = %assert.ok180
  ret i32 0
}

define internal void @sere.module.init() {
entry:
  %0 = load i8, ptr @sere.module.init.done, align 1
  %1 = icmp eq i8 %0, 0
  br i1 %1, label %init, label %end

init:                                             ; preds = %entry
  store i8 1, ptr @sere.module.init.done, align 1
  br label %end

end:                                              ; preds = %init, %entry
  ret void
}

declare i32 @sere_has_error()

declare void @sere_free(ptr)

declare void @sere_shared_release(ptr)

declare ptr @sere_str_i32_data(i32, ptr)

declare ptr @sere_str_concat_data(ptr, i64, ptr, i64, ptr)

declare i32 @sere_str_cmp(ptr, i64, ptr, i64)

declare void @sere_raise(ptr, ptr, i64)

declare ptr @sere_alloc(i64)

declare ptr @sere_shared_new(i64)

declare i64 @sere_list_len(ptr)

declare void @sere_write_nl()

declare void @sere_write(ptr, i64)

declare ptr @sere_str_f64_data(double, ptr)

declare ptr @sere_str_bool_data(i8, ptr)

declare void @sere_str_repeat(ptr, i64, i64, ptr, ptr)

declare ptr @sere_list_new(i64)

declare ptr @sere_array_new(i64, i64)

declare void @sere_list_push(ptr, ptr)

declare ptr @sere_list_item(ptr, i64)

declare ptr @sere_list_repeat(ptr, i64)

define internal { ptr, i64 } @"__sere_repr_list[i32]"(ptr %0) {
entry:
  %1 = call i64 @sere_list_len(ptr %0)
  %2 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } { ptr @34, i64 1 }, ptr %2, align 8
  br label %loop

loop:                                             ; preds = %item, %entry
  %3 = phi i64 [ 0, %entry ], [ %30, %item ]
  %4 = icmp slt i64 %3, %1
  br i1 %4, label %item, label %end

item:                                             ; preds = %loop
  %5 = icmp eq i64 %3, 0
  %6 = select i1 %5, { ptr, i64 } { ptr @36, i64 0 }, { ptr, i64 } { ptr @35, i64 2 }
  %7 = load { ptr, i64 }, ptr %2, align 8
  %cat.len = alloca i64, align 8
  %8 = extractvalue { ptr, i64 } %7, 0
  %9 = extractvalue { ptr, i64 } %7, 1
  %10 = extractvalue { ptr, i64 } %6, 0
  %11 = extractvalue { ptr, i64 } %6, 1
  %12 = call ptr @sere_str_concat_data(ptr %8, i64 %9, ptr %10, i64 %11, ptr %cat.len)
  %13 = load i64, ptr %cat.len, align 4
  %14 = insertvalue { ptr, i64 } undef, ptr %12, 0
  %15 = insertvalue { ptr, i64 } %14, i64 %13, 1
  %16 = call ptr @sere_list_item(ptr %0, i64 %3)
  %17 = load i32, ptr %16, align 4
  %str.len = alloca i64, align 8
  %18 = call ptr @sere_str_i32_data(i32 %17, ptr %str.len)
  %19 = load i64, ptr %str.len, align 4
  %20 = insertvalue { ptr, i64 } undef, ptr %18, 0
  %21 = insertvalue { ptr, i64 } %20, i64 %19, 1
  %cat.len1 = alloca i64, align 8
  %22 = extractvalue { ptr, i64 } %15, 0
  %23 = extractvalue { ptr, i64 } %15, 1
  %24 = extractvalue { ptr, i64 } %21, 0
  %25 = extractvalue { ptr, i64 } %21, 1
  %26 = call ptr @sere_str_concat_data(ptr %22, i64 %23, ptr %24, i64 %25, ptr %cat.len1)
  %27 = load i64, ptr %cat.len1, align 4
  %28 = insertvalue { ptr, i64 } undef, ptr %26, 0
  %29 = insertvalue { ptr, i64 } %28, i64 %27, 1
  store { ptr, i64 } %29, ptr %2, align 8
  %30 = add i64 %3, 1
  br label %loop

end:                                              ; preds = %loop
  %31 = load { ptr, i64 }, ptr %2, align 8
  %cat.len2 = alloca i64, align 8
  %32 = extractvalue { ptr, i64 } %31, 0
  %33 = extractvalue { ptr, i64 } %31, 1
  %34 = call ptr @sere_str_concat_data(ptr %32, i64 %33, ptr @37, i64 1, ptr %cat.len2)
  %35 = load i64, ptr %cat.len2, align 4
  %36 = insertvalue { ptr, i64 } undef, ptr %34, 0
  %37 = insertvalue { ptr, i64 } %36, i64 %35, 1
  ret { ptr, i64 } %37
}

declare ptr @sere_list_concat(ptr, ptr)

define i32 @main(i32 %0, ptr %1) {
entry:
  call void @sere_mod_init()
  call void @sere.module.init()
  %2 = call ptr @sere_list_from_argv(i32 %0, ptr %1)
  %3 = call i32 @sere_main(ptr %2)
  call void @sere_error_unhandled()
  ret i32 %3
}

define weak void @sere_mod_init() {
entry:
  ret void
}

declare ptr @sere_list_from_argv(i32, ptr)

declare void @sere_error_unhandled()
