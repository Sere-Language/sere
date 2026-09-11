; ModuleID = '_scratch\optional_alias.sere'
source_filename = "_scratch\\optional_alias.sere"
target triple = "x86_64-pc-windows-msvc"

%Exception = type { i32, { ptr, i64 } }
%Point = type { i32, i32, i32 }

@sere.module.init.done = internal global i8 0
@0 = private unnamed_addr constant [4 x i8] c"ada\00", align 1
@1 = private unnamed_addr constant [2 x i8] c" \00", align 1
@2 = private unnamed_addr constant [3 x i8] c"ok\00", align 1

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

define void @Point___init__(ptr %self, i32 %x, i32 %y) {
entry:
  %self.slot = alloca ptr, align 8
  store ptr %self, ptr %self.slot, align 8
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %y2 = alloca i32, align 4
  store i32 %y, ptr %y2, align 4
  %0 = load ptr, ptr %self.slot, align 8
  %1 = getelementptr inbounds nuw %Point, ptr %0, i32 0, i32 1
  %2 = load i32, ptr %x1, align 4
  store i32 %2, ptr %1, align 4
  %3 = load ptr, ptr %self.slot, align 8
  %4 = getelementptr inbounds nuw %Point, ptr %3, i32 0, i32 2
  %5 = load i32, ptr %y2, align 4
  store i32 %5, ptr %4, align 4
  ret void
}

define i1 @accept_optional({ i32, i64 } %value) {
entry:
  %value1 = alloca { i32, i64 }, align 8
  store { i32, i64 } %value, ptr %value1, align 4
  %0 = load { i32, i64 }, ptr %value1, align 4
  %1 = call i32 @sere_has_error()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i1 false

err.ok:                                           ; preds = %entry
  ret i1 false
}

define i32 @accept_name({ i32, i64 } %value) {
entry:
  %value1 = alloca { i32, i64 }, align 8
  store { i32, i64 } %value, ptr %value1, align 4
  %0 = call i32 @sere_has_error()
  %1 = icmp ne i32 %0, 0
  br i1 %1, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  ret i32 0
}

define i32 @accept_type(i32 %cls) {
entry:
  %cls1 = alloca i32, align 4
  store i32 %cls, ptr %cls1, align 4
  %0 = call i32 @sere_has_error()
  %1 = icmp ne i32 %0, 0
  br i1 %1, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  ret i32 0
}

define %Point @accept_typed(i32 %cls) {
entry:
  %cls1 = alloca i32, align 4
  store i32 %cls, ptr %cls1, align 4
  %init.tmp = alloca %Point, align 8
  store %Point zeroinitializer, ptr %init.tmp, align 4
  %0 = getelementptr inbounds nuw %Point, ptr %init.tmp, i32 0, i32 0
  store i32 -358027471, ptr %0, align 4
  call void @Point___init__(ptr %init.tmp, i32 3, i32 4)
  %1 = load %Point, ptr %init.tmp, align 4
  %2 = call i32 @sere_has_error()
  %3 = icmp ne i32 %2, 0
  br i1 %3, label %err, label %err.ok

err:                                              ; preds = %entry
  ret %Point zeroinitializer

err.ok:                                           ; preds = %entry
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err2, label %err.ok3

err2:                                             ; preds = %err.ok
  ret %Point zeroinitializer

err.ok3:                                          ; preds = %err.ok
  ret %Point %1
}

define i32 @accept_both({ i32, i64 } %value) {
entry:
  %value1 = alloca { i32, i64 }, align 8
  store { i32, i64 } %value, ptr %value1, align 4
  %0 = call i32 @sere_has_error()
  %1 = icmp ne i32 %0, 0
  br i1 %1, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  ret i32 0
}

define i32 @sere_main() {
entry:
  call void @sere.module.init()
  %missing = alloca { i32, i64 }, align 8
  store { i32, i64 } zeroinitializer, ptr %missing, align 4
  %present = alloca { i32, i64 }, align 8
  store { i32, i64 } { i32 1, i64 7 }, ptr %present, align 4
  %name = alloca { i32, i64 }, align 8
  store { i32, i64 } { i32 1, i64 0 }, ptr %name, align 4
  %p = alloca %Point, align 8
  %0 = call %Point @accept_typed(i32 -358027471)
  %1 = call i32 @sere_has_error()
  %2 = icmp ne i32 %1, 0
  br i1 %2, label %err, label %err.ok

err:                                              ; preds = %entry
  ret i32 0

err.ok:                                           ; preds = %entry
  store %Point %0, ptr %p, align 4
  %3 = call i32 @accept_type(i32 -358027471)
  %4 = call i32 @sere_has_error()
  %5 = icmp ne i32 %4, 0
  br i1 %5, label %err1, label %err.ok2

err1:                                             ; preds = %err.ok
  ret i32 0

err.ok2:                                          ; preds = %err.ok
  %6 = load { i32, i64 }, ptr %missing, align 4
  %7 = call i1 @accept_optional({ i32, i64 } %6)
  %8 = call i32 @sere_has_error()
  %9 = icmp ne i32 %8, 0
  br i1 %9, label %err3, label %err.ok4

if.end:                                           ; preds = %if.next, %err.ok10
  %10 = load { i32, i64 }, ptr %name, align 4
  %11 = call i32 @accept_name({ i32, i64 } %10)
  %12 = call i32 @sere_has_error()
  %13 = icmp ne i32 %12, 0
  br i1 %13, label %err11, label %err.ok12

err3:                                             ; preds = %err.ok2
  ret i32 0

err.ok4:                                          ; preds = %err.ok2
  br i1 %7, label %log.rhs, label %log.end

log.rhs:                                          ; preds = %err.ok4
  %14 = load { i32, i64 }, ptr %present, align 4
  %15 = call i1 @accept_optional({ i32, i64 } %14)
  %16 = call i32 @sere_has_error()
  %17 = icmp ne i32 %16, 0
  br i1 %17, label %err5, label %err.ok6

log.end:                                          ; preds = %err.ok6, %err.ok4
  %log.phi = phi i1 [ %7, %err.ok4 ], [ %18, %err.ok6 ]
  br i1 %log.phi, label %if.then, label %if.next

err5:                                             ; preds = %log.rhs
  ret i32 0

err.ok6:                                          ; preds = %log.rhs
  %18 = xor i1 %15, true
  br label %log.end

if.then:                                          ; preds = %log.end
  call void @sere_write(ptr @2, i64 2)
  call void @sere_write_nl()
  %19 = call i32 @sere_has_error()
  %20 = icmp ne i32 %19, 0
  br i1 %20, label %err7, label %err.ok8

if.next:                                          ; preds = %log.end
  br label %if.end

err7:                                             ; preds = %if.then
  ret i32 0

err.ok8:                                          ; preds = %if.then
  %21 = call i32 @sere_has_error()
  %22 = icmp ne i32 %21, 0
  br i1 %22, label %err9, label %err.ok10

err9:                                             ; preds = %err.ok8
  ret i32 0

err.ok10:                                         ; preds = %err.ok8
  br label %if.end

err11:                                            ; preds = %if.end
  ret i32 0

err.ok12:                                         ; preds = %if.end
  %23 = call i32 @accept_both({ i32, i64 } { i32 0, i64 5 })
  %24 = call i32 @sere_has_error()
  %25 = icmp ne i32 %24, 0
  br i1 %25, label %err13, label %err.ok14

err13:                                            ; preds = %err.ok12
  ret i32 0

err.ok14:                                         ; preds = %err.ok12
  %26 = getelementptr inbounds nuw %Point, ptr %p, i32 0, i32 1
  %27 = load i32, ptr %26, align 4
  %28 = call i32 @sere_has_error()
  %29 = icmp ne i32 %28, 0
  br i1 %29, label %err15, label %err.ok16

err15:                                            ; preds = %err.ok14
  ret i32 0

err.ok16:                                         ; preds = %err.ok14
  ret i32 %27
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

declare ptr @sere_alloc(i64)

declare ptr @sere_shared_new(i64)

declare i64 @sere_list_len(ptr)

declare void @sere_write_nl()

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
