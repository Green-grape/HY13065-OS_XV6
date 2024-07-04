#include "types.h" //user.h에 필요한 타입이 있음
#include "user.h"

int main(int argc, char* argv[]){
    const int school_id=2019089270;
    printf(1,"My student id is %d\n",school_id);
    printf(1,"My pid is %d\n",getpid());
    printf(1,"My gpid is %d\n",getgpid());
    exit();
}