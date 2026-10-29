'''
gg = lambda x: x * 2
print(gg(5))

def yz(r):
    return r * 2 *3.14

def ym(r):
    return r ** 2 * 3.14
'''
'''
class circle:
    def __init__(self,r):
        self.r = r
    def yz(self):
        return self.r * 2 * 3.14
    def ym(self):
        return self.r ** 2 * 3.14
'''
#定义一个类stu,实例的属性：name，age 实例的方法：输出学生信息用（我是一名学生 名字是张三 年龄是18）
'''
class stu:
    def __init__(self, name, age):
        self.name = name
        self.age = age

    def display_info(self):
        print(f"我是一名学生 名字是{self.name} 年龄是{self.age}")

mm = stu("张三", 18)
mm.display_info()
'''  
'''
# 定义一个父类person 类属性是country = 中国 实例属性是name age
#实例方法是show（）输出一句话 我是中国人 我的名字是xxx 年龄是xxx
#定义一个子类student 继承person类 新增实例属性stu_id（学号）
#重写show（）输出一句话 我是中国人 我的名字是xxx 年龄是xxx 我的学号是xxx
class person:
    country = "中国"

    def __init__(self, name, age):
        self.name = name
        self.age = age

    def show(self):
        print(f"我是{self.country}人 我的名字是{self.name} 年龄是{self.age}")

class student(person):
    def __init__(self, name, age, stu_id):
        super().__init__(name, age)
        self.stu_id = stu_id

    def show(self):
        print(f"我是{self.country}人 我的名字是{self.name} 年龄是{self.age} 我的学号是{self.stu_id}")

student1 = student("李四", 20, "2023001")
student1.show()
'''
'''
class Animal:
    def sound(self):
        print("动物发出声音")
class Dog(Animal):
    def sound(self):
        print("汪汪汪")
class Cat(Animal):
    def sound(self):
        print("喵喵喵")
AA = Animal()
AA.sound()
dog = Dog()
cat = Cat()
dog.sound()
cat.sound()
'''

#父类Fruit 类属性shet"生鲜货架" 实例属性name 方法show（） 输出一句话 水果真甜
#两个子类orange watemelon 继承父类 增加实例属性chandi（产地） 重写show（）输出一句话 橙子酸甜可口、柠檬果汁酸甜 产地是xxx
#用多态函数shuitian（），调用show（）方法
class Fruit:
    shelf = "生鲜货架"

    def __init__(self, name):
        self.name = name

    def show(self):
        print(f"{self.shelf}上的{self.name}真甜")

class Orange(Fruit):
    def __init__(self, name, chandi):
        super().__init__(name)
        self.chandi = chandi

    def show(self):
        print(f"{self.name}酸甜可口 产地是{self.chandi}")

class Watermelon(Fruit):
    def __init__(self, name, chandi):
        super().__init__(name)
        self.chandi = chandi

    def show(self):
        print(f"{self.name}果汁酸甜 产地是{self.chandi}")

def shuitian(fruit):
    fruit.show()

orange = Orange("橙子", "广东")
watermelon = Watermelon("西瓜", "湖南")
shuitian(orange)
shuitian(watermelon)

