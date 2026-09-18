; ModuleID = '_scratch\enum_methods.sere'
source_filename = "_scratch\\enum_methods.sere"
target triple = "x86_64-pc-windows-msvc"

%Exception = type { i32, { ptr, i64 } }

@sere.module.init.done = internal global i8 0
@0 = private unnamed_addr constant [10 x i8] c"Exception\00", align 1
@1 = private unnamed_addr constant [3 x i8] c"hi\00", align 1
@2 = private unnamed_addr constant [2 x i8] c" \00", align 1
@3 = private unnamed_addr constant [2 x i8] c" \00", align 1
@4 = private unnamed_addr constant [10 x i8] c"Exception\00", align 1

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

define i32 @Result_unwrap(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2, %err.ok8
  ret i32 0

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %value = alloca i32, align 4
  %4 = getelementptr inbounds nuw { i32 }, ptr %3, i32 0, i32 0
  %5 = load i32, ptr %4, align 4
  store i32 %5, ptr %value, align 4
  %6 = load i32, ptr %value, align 4
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %9 = icmp eq i32 %1, 1
  br i1 %9, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret i32 0

err.ok:                                           ; preds = %match.arm
  ret i32 %6

match.arm1:                                       ; preds = %match.next
  %10 = extractvalue { i32, ptr } %0, 1
  %message = alloca { ptr, i64 }, align 8
  %11 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %10, i32 0, i32 0
  %12 = load { ptr, i64 }, ptr %11, align 8
  store { ptr, i64 } %12, ptr %message, align 8
  %init.tmp = alloca %Exception, align 8
  store %Exception zeroinitializer, ptr %init.tmp, align 8
  %13 = getelementptr inbounds nuw %Exception, ptr %init.tmp, i32 0, i32 0
  store i32 -1845790122, ptr %13, align 4
  %14 = load { ptr, i64 }, ptr %message, align 8
  call void @prelude_Exception___init__(ptr %init.tmp, { ptr, i64 } %14)
  %15 = load %Exception, ptr %init.tmp, align 8
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret i32 0

err.ok4:                                          ; preds = %match.arm1
  %18 = extractvalue %Exception %15, 1
  %19 = extractvalue { ptr, i64 } %18, 0
  %20 = extractvalue { ptr, i64 } %18, 1
  call void @sere_raise(ptr @0, ptr %19, i64 %20)
  %21 = alloca %Exception, align 8
  store %Exception %15, ptr %21, align 8
  call void @sere_error_set_object(ptr %21, i64 24)
  %22 = call i32 @sere_has_error()
  %23 = icmp ne i32 %22, 0
  br i1 %23, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret i32 0

err.ok6:                                          ; preds = %err.ok4
  %24 = call i32 @sere_has_error()
  %25 = icmp ne i32 %24, 0
  br i1 %25, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret i32 0

err.ok8:                                          ; preds = %err.ok6
  br label %match.end
}

define i1 @Result_is_ok(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2
  ret i1 false

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %6 = icmp eq i32 %1, 1
  br i1 %6, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret i1 false

err.ok:                                           ; preds = %match.arm
  ret i1 true

match.arm1:                                       ; preds = %match.next
  %7 = extractvalue { i32, ptr } %0, 1
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret i1 false

err.ok4:                                          ; preds = %match.arm1
  ret i1 false
}

define i32 @sere_main() {
entry:
  call void @sere.module.init()
  %r = alloca { i32, ptr }, align 8
  %0 = call ptr @sere_alloc(i64 16)
  %1 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %0, i32 0, i32 0
  store { ptr, i64 } { ptr @1, i64 2 }, ptr %1, align 8
  %2 = insertvalue { i32, ptr } { i32 0, ptr undef }, ptr %0, 1
  %3 = call i32 @sere_has_error()
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  store { i32, ptr } %2, ptr %r, align 8
  %5 = call i1 @Result_str_is_ok(ptr %r)
  %6 = call i32 @sere_has_error()
  %7 = icmp ne i32 %6, 0
  br i1 %7, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i32 0

err.ok2:                                          ; preds = %err.ok
  %8 = zext i1 %5 to i8
  call void @sere_write_bool(i8 %8)
  call void @sere_write_nl()
  %9 = call i32 @sere_has_error()
  %10 = icmp ne i32 %9, 0
  br i1 %10, label %err3, label %err.ok4

err3:                                             ; preds = %err.ok2
  ret i32 0

err.ok4:                                          ; preds = %err.ok2
  %11 = call { ptr, i64 } @Result_str_unwrap(ptr %r)
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret i32 0

err.ok6:                                          ; preds = %err.ok4
  %14 = extractvalue { ptr, i64 } %11, 0
  %15 = extractvalue { ptr, i64 } %11, 1
  call void @sere_write(ptr %14, i64 %15)
  call void @sere_write_nl()
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret i32 0

err.ok8:                                          ; preds = %err.ok6
  %18 = call i32 @sere_has_error()
  %19 = icmp ne i32 %18, 0
  br i1 %19, label %err9, label %err.ok10

err9:                                             ; preds = %err.ok8
  ret i32 0

err.ok10:                                         ; preds = %err.ok8
  ret i32 0
}

define { ptr, i64 } @Result_str_unwrap(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2, %err.ok8
  ret { ptr, i64 } zeroinitializer

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %value = alloca { ptr, i64 }, align 8
  %4 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %3, i32 0, i32 0
  %5 = load { ptr, i64 }, ptr %4, align 8
  store { ptr, i64 } %5, ptr %value, align 8
  %6 = load { ptr, i64 }, ptr %value, align 8
  %7 = call i32 @sere_has_error()
  %8 = icmp ne i32 %7, 0
  br i1 %8, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %9 = icmp eq i32 %1, 1
  br i1 %9, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret { ptr, i64 } zeroinitializer

err.ok:                                           ; preds = %match.arm
  ret { ptr, i64 } %6

match.arm1:                                       ; preds = %match.next
  %10 = extractvalue { i32, ptr } %0, 1
  %message = alloca { ptr, i64 }, align 8
  %11 = getelementptr inbounds nuw { { ptr, i64 } }, ptr %10, i32 0, i32 0
  %12 = load { ptr, i64 }, ptr %11, align 8
  store { ptr, i64 } %12, ptr %message, align 8
  %init.tmp = alloca %Exception, align 8
  store %Exception zeroinitializer, ptr %init.tmp, align 8
  %13 = getelementptr inbounds nuw %Exception, ptr %init.tmp, i32 0, i32 0
  store i32 -1845790122, ptr %13, align 4
  %14 = load { ptr, i64 }, ptr %message, align 8
  call void @prelude_Exception___init__(ptr %init.tmp, { ptr, i64 } %14)
  %15 = load %Exception, ptr %init.tmp, align 8
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret { ptr, i64 } zeroinitializer

err.ok4:                                          ; preds = %match.arm1
  %18 = extractvalue %Exception %15, 1
  %19 = extractvalue { ptr, i64 } %18, 0
  %20 = extractvalue { ptr, i64 } %18, 1
  call void @sere_raise(ptr @4, ptr %19, i64 %20)
  %21 = alloca %Exception, align 8
  store %Exception %15, ptr %21, align 8
  call void @sere_error_set_object(ptr %21, i64 24)
  %22 = call i32 @sere_has_error()
  %23 = icmp ne i32 %22, 0
  br i1 %23, label %err5, label %err.ok6

err5:                                             ; preds = %err.ok4
  ret { ptr, i64 } zeroinitializer

err.ok6:                                          ; preds = %err.ok4
  %24 = call i32 @sere_has_error()
  %25 = icmp ne i32 %24, 0
  br i1 %25, label %err7, label %err.ok8

err7:                                             ; preds = %err.ok6
  ret { ptr, i64 } zeroinitializer

err.ok8:                                          ; preds = %err.ok6
  br label %match.end
}

define i1 @Result_str_is_ok(ptr %self) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %0 = load { i32, ptr }, ptr %self.slot, align 8
  %1 = extractvalue { i32, ptr } %0, 0
  %2 = icmp eq i32 %1, 0
  br i1 %2, label %match.arm, label %match.next

match.end:                                        ; preds = %match.next2
  ret i1 false

match.arm:                                        ; preds = %entry
  %3 = extractvalue { i32, ptr } %0, 1
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err, label %err.ok

match.next:                                       ; preds = %entry
  %6 = icmp eq i32 %1, 1
  br i1 %6, label %match.arm1, label %match.next2

err:                                              ; preds = %match.arm
  ret i1 false

err.ok:                                           ; preds = %match.arm
  ret i1 true

match.arm1:                                       ; preds = %match.next
  %7 = extractvalue { i32, ptr } %0, 1
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err3, label %err.ok4

match.next2:                                      ; preds = %match.next
  br label %match.end

err3:                                             ; preds = %match.arm1
  ret i1 false

err.ok4:                                          ; preds = %match.arm1
  ret i1 false
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

declare void @sere_raise(ptr, ptr, i64)

declare void @sere_error_set_object(ptr, i64)

declare ptr @sere_alloc(i64)

declare ptr @sere_shared_new(i64)

declare i64 @sere_list_len(ptr)

declare void @sere_write_nl()

declare void @sere_write_bool(i8)

declare void @sere_write(ptr, i64)

define i32 @main(i32 %0, ptr %1) {
entry:
  call void @sere_mod_init()
  call void @sere.module.init()
  %2 = call i32 @sere_main()
  call void @sere_error_unhandled()
  ret i32 %2
}

define weak void @sere_mod_init() {
entry:
  ret void
}

declare void @sere_error_unhandled()
