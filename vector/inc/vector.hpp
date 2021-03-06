
/** 
 *  @file   vector.hpp
 *  @brief  Vector implementation header.
 *
 *  This file contains the Header Code for the vector Implementation.
 *
 *  @author         Tony Josi   https://tonyjosi97.github.io/profile/
 *  @copyright      Copyright (C) 2020 Tony Josi
 *  @bug            No known bugs.
 */

#ifndef __VECT_IMPL__HEADER__
#define __VECT_IMPL__HEADER__

#include <type_traits>
#include <memory>
#include <stdexcept>
#include <iterator>
#include <algorithm>
#include <iostream>

/* namespace: Reinventing the wheel (rtw) vector */
namespace rtw_vect {

#define     __VECT_IMPL_DEBUG_OP 

#ifdef      __VECT_IMPL_DEBUG_OP 
    #define     _rtw_DEBUG_OP(_str__)    std::cout<<_str__;
#else
    #define     _rtw_DEBUG_OP(_str__)
#endif      /* __VECT_IMPL_DEBUG_OP */

    /* Vector class */
    template <typename T>
    class vector {

        public:
            
            /* Constructors, Assignment operators & Destructor. */

            vector(std::size_t sz = 5)
                :size__         {     sz  }
                ,cur_len__      {     0   }
                ,mem_buff__     {     static_cast<T *>(::operator new(sizeof(T) * size__))   } {

                    /* 
                    operator new is used instead of new operator because,
                    operator new wont call the constructor for T during allocation
                    and destructor for T during deallocation. 
                    
                    This infact will fix a major error caused due to
                    multiple call to destructors on the same object, 
                    which can occur in different scenarios as the delete[] will also
                    call the destructor on all elements regardless of whether 
                    it was destroyed previously, say, for example via pop_back().

                    A possible scenario that causes above issue:

                    1. Consider a type T, that has a pointer to heap memory and the 
                    destructor ( ~T() ) of T will delete the heap memory as usual. 
                    2. If vector<T> calls pop_back it will call the destructor of T
                    on the last element of the vector.
                    3. Also during the destruction of the vector, the delete[] will 
                    again call destructor of T for the same object, which was deleted
                    in step 2. This happens for example when vector goes out of scope or
                    destroyed, soon after a pop_back.
                    4. Thus delete is called twice on the same memory location 
                    causing undefined behaviour.

                                    -- https://stackoverflow.com/q/17344727/6792356

                    */

                    _rtw_DEBUG_OP("Default ctor: "<<size__<<"\n");
            }

            vector(vector const &rhs)
                :size__         {   rhs.size__  }
                ,cur_len__      {   0           }
                ,mem_buff__     {   static_cast<T *>(::operator new(sizeof(T) * size__))    } {

                try {

                    /* push_back each element to the destination object (lhs) 
                    from rhs object */
                    for(std::size_t i = 0; i < rhs.cur_len__; ++i)
                        push_back(rhs.mem_buff__[i]);
                
                }
                catch(...) {
                    /* Delete mem_buff__  before throwing, uses t_buff_destrutor() functor to 
                    delete the allocated memory */
                    std::unique_ptr<T, t_buff_destructor>      dctor_obj(mem_buff__, t_buff_destructor());

                    /* SFINAE based overload for destroying individual elements */
                    destroy_items<T>();

                    /* finally throw exception */
                    throw;

                }

                _rtw_DEBUG_OP("Copy ctor\n");

            }

            vector& operator=(vector const &rhs) {

                _rtw_DEBUG_OP("Copy assign. ctor\n");
                
                /* SFINAE based overload for copying elements based on T */
                copy_assign<T>(rhs);
                return *this;
            
            }

            vector(vector &&rhs)                    noexcept 
                :size__         {   0   }
                ,cur_len__      {   0   }
                ,mem_buff__     {   nullptr } {

                /* Swap the resources so that rhs object will have 
                no resources pointed by it and also reset the cur_len__ to zero as the destroy_items()
                won't try to free those moved elements. */
                rhs.swap(*this);
                _rtw_DEBUG_OP("Move ctor\n");

            }

            vector& operator=(vector &&rhs)         noexcept {

                rhs.swap(*this);
                _rtw_DEBUG_OP("Move assign. ctor\n");

                /* Return the current object. */
                return *this;

            }

            vector(std::initializer_list<T> const &i_list)
                :size__         {   static_cast<std::size_t>(std::distance(std::begin(i_list), std::end(i_list)))     }
                ,cur_len__      {   0   }
                ,mem_buff__     {   static_cast<T *>(::operator new(sizeof(T) * size__))    } {
                    
                    /* Calculates number of elements in the list,
                    allocates memory, and copy those elements  to
                    the object.
                    */

                    /*
                    std::begin:

                    template< class C > 
                    auto begin( C& c ) -> decltype(c.begin());
                    */

                    for(auto i = std::begin(i_list); i < std::end(i_list); ++i)
                        push_back_copy__(*i);

            }

            ~vector() {

                /* Using std::unique_ptr for ensuring resource
                deallocation even if exception occur during the destruction of
                individual vector elements. 
                NOTE: For std::unique_ptr<T> the type doesn't need pointer operator (*) 
                as type inside std::unique_ptr class itself is a pointer. */
                std::unique_ptr<T, t_buff_destructor>   dctor_obj(mem_buff__, t_buff_destructor());
                destroy_items<T>();
                _rtw_DEBUG_OP("Dctor\n");

            }


            void swap(vector &rhs)                  noexcept {

                using std::swap;
                swap(size__,        rhs.size__);
                swap(cur_len__,     rhs.cur_len__);
                swap(mem_buff__,    rhs.mem_buff__);
            
            }

            /* Public member functions */

            void push_back(T const &val) {

                if(cur_len__ == size__)
                    resize__();

                /* Use internal push back function
                that uses copy constructor of T to copy the input
                opject 'val' into the heap memory allocated to the vector. */
                push_back_copy__(val);

            }

            void push_back(T &&val) {

                if(cur_len__ == size__)
                    resize__();

                /* Use internal push back function
                that uses move constructor of T to move the input
                opject 'val' into the heap memory allocated to the vector. */
                
                /* 
                
                if 'std::forward' is not used val will be deduced as a lvalue. 

                "all named values (such as function parameters) always 
                evaluate as lvalues (even those declared as rvalue references)"
                    -- http://www.cplusplus.com/reference/utility/forward/ 
                
                */
                push_back_move__(std::forward<T>(val));

            }

            template <typename... Args>
            void emblace_back(Args &&... args) {

                if(cur_len__ == size__)
                    resize__();

                /* Use internal push back emblace function
                that uses normal constructor of T to create the new
                opject directly into the heap memory allocated to the vector
                based on the variadic arguments 'args'. */
                /* 'std::forward' is used for perfect forwarding the arguments to deduced types. */
                push_back_emblace__(std::forward<Args>(args)...);

            }

            void pop_back() {

                if(cur_len__ > 0) {
                    --cur_len__;
                    /* Calling T's destructor on the deleted object. */
                    mem_buff__[cur_len__].~T();
                } else
                    _rtw_DEBUG_OP("No popback -> cur_len__ <= 0\n");
                

            }


            /* Reserve memory */

            void reserve(std::size_t new_sz) {

                /* Reserve only if the new size is greater
                than the current size. */
                if(size__ < new_sz)
                    reserve_n_copy__(new_sz);

            }


            /* Array operators */

            T & operator[](std::size_t idx) {
                return mem_buff__[idx];
            }

            T const & operator[](std::size_t idx)   const {
                return mem_buff__[idx];
            }


            /* Validated access */

            T & at(std::size_t idx) {

                if(cur_len__ <= idx)
                    throw std::out_of_range("RTW::VECTOR out of range");

                return mem_buff__[idx];

            }

            T const & at(std::size_t idx)           const {

                if(cur_len__ <= idx)
                    throw std::out_of_range("RTW::VECTOR out of range");

                return mem_buff__[idx];

            }

            /* Status & Comparisson Functions/Operators */

            std::size_t size()                      const {

                return cur_len__;

            } 

            bool empty()                            const {

                return cur_len__ == 0;

            }

            bool operator==(const vector<T> &rhs)   const {

                _rtw_DEBUG_OP("Uses ==\n");
                return ((size() == rhs.size()) && \
                (std::equal(mem_buff__, mem_buff__ + cur_len__, rhs.mem_buff__)));
                
            }

            bool operator!=(const vector<T> &rhs)   const {

                return !(*this == rhs);

            }

        private:

            /* Internal data members */

            std::size_t     size__;
            std::size_t     cur_len__;
            T              *mem_buff__;


            /* Internal helper member functions */

            /*               
                                    S F I N A E  Based  overload  deduction
                                    ---------------------------------------

            Substitution failure is not an error (SFINAE) - invalid substitution of 
            template parameters is not in itself an error. 

            std::enable_if relies on the SFINAE principle to deduce whether given overloaded
            functions are considered potential overload candidate set.  

            To make SFINAE work, the functions should be template functions.

            std::enable_if::type is defined only if the condition (first argument 
            inside the <> of the std::enable_if) is true, thus SFINAE removes those
            funtion declarations/definitions whose types are not defined, 
            from the potential overload candidate set. Thus, exactly one function
            will be matching at compile time.  

                            -- http://www.cplusplus.com/reference/type_traits/enable_if/

            The template parameter name T cannot be reused here because: A template-parameter 
            shall not be redeclared within its scope (including nested scopes). 
            A template- parameter shall not have the same name as the template name.
            
            */

            template <typename U>
            typename std::enable_if <(std::is_trivially_destructible <U>::value == false), void>::type
            destroy_items() {

                /* Call destructor of each element in reverse order. */
                for(std::size_t i = cur_len__; i > 0; --i)
                    mem_buff__[i - 1].~T();
            
            }

            template <typename U>
            typename std::enable_if <(std::is_trivially_destructible <U>::value == true), void>::type
            destroy_items() {

                /* Nothing to do, as each elements are trivially destructible */
                return;
            
            }

            template <typename U>
            typename std::enable_if <((std::is_nothrow_destructible <U>::value &&
            std::is_nothrow_copy_constructible <U>::value) == true), void>::type
            copy_assign(vector<T> const &rhs) {

                /* if the vector type, T is_nothrow_copy_constructible and
                is_trivially_destructible then reuse memory if available else
                do sperate memory allocation. */

                /* check for self comparisson */
                if(this == &rhs)
                    return;

                if(size__ >= rhs.cur_len__) {
                    /* clear the previous elements */
                    destroy_items<T>();
                    /* resuse the destination memory */
                    cur_len__ = 0;
                    for(std::size_t i = 0; i < rhs.cur_len__; ++i)
                        push_back_copy__(rhs[i]);
                }
                else {
                    /* If there isnt enough memory then allocate again */
                    vector<T> temp_copy(rhs);
                    temp_copy.swap(*this);
                }

            }

            template <typename U>
            typename std::enable_if <((std::is_nothrow_destructible <U>::value &&
            std::is_nothrow_copy_constructible <U>::value) == false), void>::type
            copy_assign(vector<T> const &rhs) {
                
                /* if the vector type, T cannot be nothrow_copy_constructible 
                then do sperate memory allocation, the usual way. */
                vector<T> temp_copy(rhs);
                temp_copy.swap(*this);

            }

            template <typename U>
            typename std::enable_if <(std::is_nothrow_move_constructible <U>::value == true), void>::type
            copy_items(vector<T> &dest) {
                
                /* if the vector type, T is nothrow_move_constructible 
                then move the elements to the buffer */
                std::for_each(mem_buff__, mem_buff__ + cur_len__, [&dest](T &item){dest.push_back_move__(std::move(item));});

            }

            template <typename U>
            typename std::enable_if <(std::is_nothrow_move_constructible <U>::value == false), void>::type
            copy_items(vector<T> &dest) {

                /* if the vector type, T is not nothrow_move_constructible 
                then copy the elements to the buffer */
                std::for_each(mem_buff__, mem_buff__ + cur_len__, [&dest](T const &item){dest.push_back_copy__(item);});
                
            }

            /* Initialisation of memory using copy constructor of T. */
            void push_back_copy__(T const &val) {

                /* Initialise T object in location (mem_buff__ + cur_len__) 
                using placement new and copy constructor of T. */
                new (mem_buff__ + cur_len__) T(val);
                ++cur_len__;

            }

            /* Initialisation of memory using move constructor of T. */
            void push_back_move__(T &&val) {

                /* Initialise T object in location (mem_buff__ + cur_len__) 
                using placement new and move constructor of T. */
                new (mem_buff__ + cur_len__) T(std::move(val));
                ++cur_len__;

            }

            template <typename... Args>
            void push_back_emblace__(Args &&... args) {

                /* Initialise T object in location (mem_buff__ + cur_len__) 
                using placement new and normal constructor of T using
                variadic arguments 'args'. */
                /* 'std::forward' is used for perfect forwarding the arguments to deduced types. */
                new (mem_buff__ + cur_len__) T(std::forward<Args>(args)...);
                ++cur_len__;

            }

            void resize__() {

                /* Calculate new vector size and do allocation and 
                copying of old data back to resized vector. */
                std::size_t new_size = std::max(static_cast<std::size_t>(2), size__ * 2);
                reserve_n_copy__(new_size);

            }

            void reserve_n_copy__(std::size_t new_sz) {

                /* Create temporary vector with new size. */
                vector<T> temp_vect(new_sz);

                /* Copy previous items to the temporary vector 
                and swap with *this. */
                copy_items<T>(temp_vect);
                temp_vect.swap(*this);

            }

            /* Functor for destroying the memory buffer */
            struct t_buff_destructor {

                void operator()(T *buff)    const {
                    ::operator delete(buff);
                }

            };
    };
}

#endif /* __VECT_IMPL__HEADER__ */